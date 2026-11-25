// =====================================================================
//  UFox GUI shader  (Slang -> SPIR-V, entries: vertMain / fragMain)
//
//  One instanced quad per GUI element. Each instance reads its data from
//  two storage buffers (per-element + per-style) and draws:
//      background color  ->  image / MSDF glyph  ->  border ring
//  all inside a rounded rectangle computed analytically with SDFs.
// =====================================================================

// Per-vertex data of the shared unit quad (mesh Vertex layout).
struct VSInput {
    float3 inPos;
    float3 inNormal;
    float2 inTexCoord;
    float3 inColor;
};

// Shape data prepared on the CPU (MakeSDRectInputData). Everything is in
// pixels and centered on the shape, because the SDF works around (0,0).
struct SDRectInputData {
    float4 offsetAndTiling;   // xy = half element size (centers the quad), zw = full element size
    float2 size;              // outer half-size  (element half-size minus margin)
    float2 pOffset;           // shifts the outer box to account for unequal margins
    float2 iSize;             // inner half-size  (outer half-size minus border)
    float2 iPOffset;          // shifts the inner box to account for unequal borders
};


// Per-element instance data (SSBO binding 0). 160 bytes, must match C++ ElementData.
struct ElementData {
    float4x4        model;
    float4          imageOffsetAndTiling;
    SDRectInputData sdInput;
    float2          scrollingOffset;
    uint            styleIndex;
    uint            imageIndex;
    uint            tailPadding[2];
};

// Deduplicated style palette entry (SSBO binding 1). 144 bytes, must match C++.
// Radius vec4 order is (bottom-right, top-right, bottom-left, top-left).
struct StyleStaticData {
    float4 radius;
    float4 iRadius;
    float4 imageColor;
    float4 backgroundColor;
    float4 borderBottomColor;
    float4 borderTopColor;
    float4 borderLeftColor;
    float4 borderRightColor;

    uint  imageRepeatMode;
    uint  isFont;
    uint  tailPadding[2];
};


// Screen projection (UBO binding 2): pixel space -> clip space.
struct ScreenData {
    float4x4 viewProjection;
};

[[vk::binding(0, 0)]] StructuredBuffer<ElementData>   elementBuffer;
[[vk::binding(1, 0)]] StructuredBuffer<StyleStaticData> styleBuffer;
[[vk::binding(2, 0)]] ConstantBuffer<ScreenData>      sb;

// Bindless-style arrays: textures and samplers are separate descriptors,
// so any texture can be paired with any sampler (repeat mode picks the sampler).
[[vk::binding(3, 0)]] Texture2D    textureArray[];
[[vk::binding(4, 0)]] SamplerState guiSampler[];
//[[vk::binding(6,0)]] StructuredBuffer<GlyphGpuData> glyphBuffer[];

// Data passed to the fragment shader. The shape data is interpolated, so
// outerP / innerP give every pixel its position relative to the shape
// center, which is what the SDF functions need.
struct VSOutput {
    float4 pos : SV_Position;

    float2 imageUV;
    // Integers are constant per instance: never interpolate them.
    nointerpolation uint imageID;
    nointerpolation uint imageRepeatMode;
    nointerpolation uint isFont;

    float4 bgColor;
    float4 imageColor;
    float4 borderBottomColor;
    float4 borderTopColor;
    float4 borderLeftColor;
    float4 borderRightColor;

    float4 outerRadius;
    float2 outerSize;
    float2 outerP;

    float4 innerRadius;
    float2 innerSize;
    float2 innerP;
};

[shader("vertex")]
VSOutput vertMain(VSInput input, uint instanceID : SV_InstanceID, uint baseInstanceOffset : SV_StartInstanceLocation){
    // drawIndexed(firstInstance = ssboIndex) -> SV_StartInstanceLocation.
    // The CPU packs elements into the SSBO and selects them with the
    // first-instance offset, so we add it back to get the buffer slot.
    uint instanceIndex = instanceID + baseInstanceOffset;

    ElementData el = elementBuffer[instanceIndex];
    StyleStaticData style = styleBuffer[el.styleIndex];
    SDRectInputData sd = el.sdInput;

    VSOutput o;
    // model scales/moves the unit quad to the element rect (pixels),
    // viewProjection converts pixels to clip space.
    o.pos = mul(sb.viewProjection, mul(el.model, float4(input.inPos, 1.0)));

    // Colors
    o.bgColor           = style.backgroundColor;
    o.imageColor        = style.imageColor;
    o.borderBottomColor = style.borderBottomColor;
    o.borderTopColor    = style.borderTopColor;
    o.borderLeftColor   = style.borderLeftColor;
    o.borderRightColor  = style.borderRightColor;

    // Shape Data
    o.outerRadius = style.radius;
    o.outerSize   = sd.size;
    // Position of this vertex relative to the shape center, in pixels:
    //   uv * fullSize - halfSize  ->  [-half, +half]
    // plus pOffset for margins. Interpolated per pixel by the rasterizer.
    o.outerP      = input.inTexCoord * sd.offsetAndTiling.zw - sd.offsetAndTiling.xy + sd.pOffset;

    o.innerRadius = style.iRadius;
    o.innerSize   = sd.iSize;
    // Same point in the inner box space (shifted by the border asymmetry).
    // selectBorderColor uses outerP - innerP to recover border thickness.
    o.innerP      = o.outerP + sd.iPOffset;

    // Texture UV: offset/tiling are precomputed on the CPU for the
    // image scale mode (fit / crop / stretch) and alignment.
    o.imageUV         = (input.inTexCoord + el.imageOffsetAndTiling.xy) * el.imageOffsetAndTiling.zw;
    o.imageID         = el.imageIndex;
    o.imageRepeatMode = style.imageRepeatMode;
    o.isFont          = style.isFont;

    return o;
}

// Rounded Box SDF (signed distance, negative inside).
//   p : point relative to the box center
//   b : box half-size
//   r : corner radii (BR, TR, BL, TL)
// First two lines pick the radius of the quadrant p is in; the rest is the
// classic "shrink the box by r, take the distance, subtract r" formula.
float sdRoundedBox(float2 p, float2 b, float4 r){
    r.xy = (p.x > 0.0) ? r.xy : r.zw;
    r.x  = (p.y > 0.0) ? r.x  : r.y;
    float2 q = abs(p) - b + r.x;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r.x;
}

// Must equal PX_RANGE on the C++ side (msdf-atlas-gen pixel range).
static const float PX_RANGE = 8.0;

// Median of the three MSDF channels = the actual signed distance.
float median3(float r, float g, float b){
    return max(min(r, g), min(max(r, g), b));
}

// Edge anti-aliasing half-width in pixels. A 1px linear ramp centered on
// the edge: 50% coverage exactly on the shape boundary (no shrink/grow).
// Scale this by your DPI factor if you add high-DPI support.
static const float EDGE_AA = 0.5;

// Renders one MSDF glyph: reconstruct the distance, then turn it into
// coverage using how many screen pixels one texel covers, so edges stay
// sharp and anti-aliased at any font size.
//
// uvDx / uvDy are the screen-space UV derivatives computed ONCE at the top
// of fragMain (uniform control flow). Using SampleGrad with them means
// nothing in here depends on implicit derivatives, so it is safe to call
// from inside a branch.
float4 sampleGlyph(VSOutput i, float2 uvDx, float2 uvDy){
    // Non-uniform index: imageID differs per instance inside one draw.
    Texture2D tex = textureArray[NonUniformResourceIndex(i.imageID)];

    float2 texSize;
    tex.GetDimensions(texSize.x, texSize.y);

    float4 s  = tex.SampleGrad(guiSampler[0], i.imageUV, uvDx, uvDy);
    float  sd = median3(s.r, s.g, s.b);

    // Screen-space pixel range (the standard msdf-atlas-gen formula)
    float2 unitRange      = PX_RANGE / texSize;
    float2 screenTexSize  = 1.0 / max(abs(uvDx) + abs(uvDy), 1e-6);   // = fwidth(imageUV)
    float  screenPxRange  = max(0.5 * dot(unitRange, screenTexSize), 1.0);

    float opacity = clamp(screenPxRange * (sd - 0.5) + 0.5, 0.0, 1.0);

    // s.a holds the true SDF (MTSDF). Use it for outlines/glow later:
    // float outline = clamp(screenPxRange * (s.a - 0.5 + outlineWidth) + 0.5, 0, 1);

    return float4(i.imageColor.rgb, opacity * i.imageColor.a);
}

// =====================================================================
//  BORDER COLOR SPLIT  (how the four border colors meet at a corner)
// =====================================================================
//
//  GOAL
//  ----
//  Each side of the border can have its own color (top / right / bottom /
//  left). At a corner two colors meet, so we need a "seam" that decides
//  which color a pixel in the corner gets.
//
//  HISTORY - HOW WE GOT HERE
//  -------------------------
//  v1 (original, CSS-like):
//      The seam is the straight line from the OUTER sharp corner of the
//      rect to the INNER sharp corner (the corner of the padding box).
//      Test:  norm.x < norm.y  (distance-to-edge / border-thickness).
//      Problem: when the corner is ROUNDED and the two border widths are
//      unequal (bx != by), that line crosses the rounded arc at an angle
//      that depends on bx:by. Example bx=4, by=12, R=40 -> the seam hits
//      the arc at ~32deg/58deg, so the arc is split ~36/64 instead of
//      50/50. The sharp corner it starts from is also outside the visible
//      (rounded) shape, so it looks wrong.
//
//  v2 (this one):
//      The seam is the line through two points picked ON THE ARCS:
//          A = the 45-degree point of the OUTER arc
//          B = the 45-degree point of the INNER arc
//      (the "A" and "B" from the sketch). Because A is always the arc's
//      midpoint, the outer arc is always split 45/45, and because B is the
//      inner arc's midpoint, the inner arc is split 50/50 too, no matter
//      what the border widths are.
//      With R = r = 0 (square corner) A = (0,0) and B = (bx,by), so it
//      collapses to the old CSS corner-to-corner line. Square corners are
//      therefore unchanged.
//
//  COORDINATE SYSTEM (corner-local, measured INWARD from the outer edges)
//  ----------------------------------------------------------------------
//
//        v (distance from the horizontal outer edge)
//        ^
//        |   inner arc
//        |  /
//        | /   B = (bx + K*r, by + K*r)
//        |/   /
//        +---/------------- inner edge (distance by)
//        |  /  A = (K*R, K*R)
//        | /
//        +-----------------> u (distance from the vertical outer edge)
//     outer corner (0,0)
//
//  K = 1 - 1/sqrt(2) = 0.29289...
//      Why: an arc of radius R has its center at (R, R). The point at 45
//      degrees is center - R/sqrt(2) * (1,1) = R*(1 - 1/sqrt(2)) * (1,1)
//      = K*R * (1,1). The same holds for the inner arc, but its center is
//      shifted by the border thickness, so B = t + K*r.
//
//  SIDE-OF-LINE TEST
//  -----------------
//  D = B - A is the seam direction. For a pixel at (u,v), q = (u,v) - A
//  is the vector from A to the pixel. The 2D cross product of q and D,
//          f = q.y * D.x - q.x * D.y
//  is the signed distance (in pixels, because D is normalized) from the
//  pixel to the seam:
//          f > 0  -> pixel is on the vertical-border side (left/right color)
//          f < 0  -> pixel is on the horizontal-border side (top/bottom color)
//  It is the same test as the old  u/bx < v/by  for the square case.

// Picks the radius of the corner the pixel is in, using the SAME quadrant
// selection as sdRoundedBox (radius vec4 order = BR, TR, BL, TL, y-down).
// The radius is clamped to the half-size of its box, exactly like a real
// rounded rect would be. If it is not clamped here, A and B would drift
// off the real arcs for large radii on small boxes.
float pickRadius(float2 p, float4 r, float2 halfSize){
    r.xy = (p.x > 0.0) ? r.xy : r.zw;          // right half -> (BR,TR), left half -> (BL,TL)
    float v = (p.y > 0.0) ? r.x : r.y;         // lower half -> first, upper half -> second
    return min(v, min(halfSize.x, halfSize.y));
}

float4 selectBorderColor(float2 outerP, float2 outerSize, float2 innerP, float2 innerSize, float4 outerRadius, float4 innerRadius,float4 topColor, float4 bottomColor,float4 leftColor, float4 rightColor){
    const float K = 0.29289322;                       // 1 - 1/sqrt(2), see derivation above

    // Which quadrant of the box is this pixel in?  (+1 / -1 per axis)
    // outerP is centered on the shape, so the sign tells right/left and bottom/top.
    float2 s = float2(outerP.x >= 0.0 ? 1.0 : -1.0,
                      outerP.y >= 0.0 ? 1.0 : -1.0);

    // Border thickness on this pixel's two sides (bx, by).
    // outerP and innerP are the same point expressed in the outer and inner
    // box spaces; their difference is the inner box offset caused by
    // unequal borders/margins. Multiplying by the sign mirrors it so the
    // result is always the thickness of the side nearest to the pixel.
    // (This is the same 't' as the original shader, just without branches.)
    float2 dIn = outerP - innerP;
    float2 t   = outerSize - innerSize - float2(s.x * dIn.x, s.y * dIn.y);
    t = max(t, 0.0);                                  // numerical safety: no negative thickness

    // Pixel position measured inward from the OUTER edges: (u, v).
    // abs() folds all four quadrants onto one, so the maths below only
    // has to be written once for the "bottom-right style" corner.
    float2 uv = outerSize - abs(outerP);

    // Radii of the corner this pixel belongs to (outer and inner arc).
    float R = pickRadius(outerP, outerRadius, outerSize);
    float r = pickRadius(outerP, innerRadius, innerSize);

    float2 A = float2(K * R);                         // 45deg point on the outer arc
    float2 B = t + K * r;                             // 45deg point on the inner arc

    // Seam direction. Forcing both components positive stops the line from
    // leaning "backwards" when one border has zero width and r < R
    // (then B.x - A.x would be negative and the seam would cut across a
    // straight edge far from the corner).
    float2 D = max(B - A, float2(1e-3));
    D = normalize(D);                                 // unit length -> f is in pixels

    // Signed pixel distance from this pixel to the seam (see side-of-line test).
    float2 q = uv - A;
    float f  = q.y * D.x - q.x * D.y;

    // Anti-aliased seam, 1 pixel wide. outerP is in screen pixels (the
    // model matrix maps the unit quad to pixels) and D is unit length, so
    // f is already a pixel distance: no fwidth needed. That also means this
    // function has no derivatives, so fragMain may skip it per pixel.
    // w = 0 -> cap color, w = 1 -> side color.
    float w  = clamp(f + EDGE_AA, 0.0, 1.0);

    // The two candidate colors for this quadrant.
    float4 side = (s.x > 0.0) ? rightColor  : leftColor;     // vertical border
    float4 cap  = (s.y > 0.0) ? bottomColor : topColor;      // horizontal border

    // A border with (almost) zero width must never win the corner.
    // Without this a thin wedge of its color could leak in near the arc.
    // 0.5 is in pixels; scale it if you render with a DPI scale != 1.
    if (t.x < 0.5) w = 0.0;                           // no side border -> cap color
    if (t.y < 0.5) w = 1.0;                           // no cap border  -> side color

    // Blend in PREMULTIPLIED alpha. Lerping straight (non-premultiplied)
    // colors with different alphas darkens/brightens the seam (fringing).
    // Then convert back to straight alpha for the SrcAlpha blend state.
    float4 pm = lerp(float4(cap.rgb * cap.a, cap.a),
                     float4(side.rgb * side.a, side.a), w);
    return float4(pm.rgb / max(pm.a, 1e-5), pm.a);
}

// ---------------------------------------------------------------------
//  Fragment: composites three layers, bottom to top
//      1. background color
//      2. image / glyph        (over the background)
//      3. border ring          (over 1+2)
//  and finally multiplies alpha by the outer rounded-rect mask.
// ---------------------------------------------------------------------
[shader("fragment")]
float4 fragMain(VSOutput i) : SV_TARGET
{
    // Derivatives must be taken in uniform control flow, so do it first,
    // before any early-out or branch below.
    float2 uvDx = ddx(i.imageUV);
    float2 uvDy = ddy(i.imageUV);

    // Outer shape: 1 inside the rounded rect, 0 outside, with a symmetric
    // 1px ramp centered on the edge (50% exactly at distance 0).
    float outerDist = sdRoundedBox(i.outerP, i.outerSize, i.outerRadius);
    float outerMask = clamp(EDGE_AA - outerDist, 0.0, 1.0);
    if (outerMask <= 0.0) return float4(0.0);          // outside the shape: nothing to draw

    // Inner shape (padding box): 0 inside it, 1 in the border ring.
    // Inverted compared to outerMask because the border lives OUTSIDE it.
    float innerDist = sdRoundedBox(i.innerP, i.innerSize, i.innerRadius);
    float innerMask = clamp(EDGE_AA + innerDist, 0.0, 1.0);

    // Image layer: glyphs use the MSDF path, everything else is a plain
    // texture sample tinted by imageColor. The sampler index = repeat mode.
    // Both indices are per-instance, hence NonUniformResourceIndex.
    float4 texColor;
    if (i.isFont != 0) {
        texColor = sampleGlyph(i, uvDx, uvDy);
    } else {
        texColor = textureArray[NonUniformResourceIndex(i.imageID)]
                       .SampleGrad(guiSampler[NonUniformResourceIndex(i.imageRepeatMode)],
                                   i.imageUV, uvDx, uvDy) * i.imageColor;
    }

    // Proper "over" compositing of image on background
    // (non-premultiplied output for the SrcAlpha blend state).
    float  aOut  = texColor.a + i.bgColor.a * (1.0 - texColor.a);
    float3 cOut  = (texColor.rgb * texColor.a + i.bgColor.rgb * i.bgColor.a * (1.0 - texColor.a)) / max(aOut, 1e-5);

    float3 fC = cOut;
    float  fA = aOut;

    // Border layer. Skipped for every pixel inside the padding box, which is
    // the large majority of pixels on big panels (the seam math includes a
    // normalize, so this matters).
    if (innerMask > 0.0) {
        float4 borderColor = selectBorderColor(i.outerP, i.outerSize, i.innerP, i.innerSize,
                                               i.outerRadius, i.innerRadius,
                                               i.borderTopColor, i.borderBottomColor,
                                               i.borderLeftColor, i.borderRightColor);

        // border over the (bg+image) result
        float bA = borderColor.a * innerMask;
        fA = bA + aOut * (1.0 - bA);
        fC = (borderColor.rgb * bA + cOut * aOut * (1.0 - bA)) / max(fA, 1e-5);
    }

    return float4(fC, fA * outerMask);
}
