struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

[[vk::push_constant]]
struct
{
    float2 iResolution;
    float iTime;
    float iTimeDelta;
    float4 iMouse;
} pc;

#define iResolution pc.iResolution
#define iTime pc.iTime
#define iMouse pc.iMouse

#define MaximumRaySteps 100
#define MaximumDistance 1000.
#define MinimumDistance .01
#define PI 3.141592653589793238

float mod(float x, float y)
{
    return x - y * floor(x / y);
}

// --------------------------------------------------------------------------------------------//
// SDF FUNCTIONS //

// Sphere
// s: radius
float SignedDistSphere(float3 p, float s) {
    return length(p) - s;
}

// Box
// b: size of box in x/y/z
float SignedDistBox(float3 p, float3 b) {
    float3 d = abs(p) - b;
    return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

// (Infinite) Plane
// n.xyz: normal of the plane (normalized)
// n.w: offset from origin
float SignedDistPlane(float3 p, float4 n) {
    return dot(p, n.xyz) + n.w;
}

// Rounded box
// r: radius of the rounded edges
float SignedDistRoundBox(in float3 p, in float3 b, in float r) {
    float3 q = abs(p) - b;
    return min(max(q.x, max(q.y, q.z)), 0.0) + length(max(q, 0.0)) - r;
}

// BOOLEAN OPERATORS //

// Union
// d1: signed distance to shape 1
// d2: signed distance to shape 2
float opU(float d1, float d2) {
    return (d1 < d2) ? d1 : d2;
}

// Subtraction
// d1: signed distance to shape 1
// d2: signed distance to shape 2
float4 opS(float4 d1, float4 d2) {
    return (-d1.w > d2.w) ? -d1 : d2;
}

// Intersection
// d1: signed distance to shape 1
// d2: signed distance to shape 2
float4 opI(float4 d1, float4 d2) {
    return (d1.w > d2.w) ? d1 : d2;
}

// Mod Position Axis
float pMod1(inout float p, float size) {
    float halfsize = size * 0.5;
    float c = floor((p + halfsize) / size);
    p = mod(p + halfsize, size) - halfsize;
    p = mod(-p + halfsize, size) - halfsize;
    return c;
}

// SMOOTH BOOLEAN OPERATORS //

// Smooth Union
// d1: signed distance to shape 1
// d2: signed distance to shape 2
// k: smoothness value for the trasition
float opUS(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    float dist = lerp(d2, d1, h) - k * h * (1.0 - h);

    return dist;
}

// Smooth Subtraction
// d1: signed distance to shape 1
// d2: signed distance to shape 2
// k: smoothness value for the trasition
float4 opSS(float4 d1, float4 d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2.w + d1.w) / k, 0.0, 1.0);
    float dist = lerp(d2.w, -d1.w, h) + k * h * (1.0 - h);
    float3 color = lerp(d2.rgb, d1.rgb, h);

    return float4(color.rgb, dist);
}

// Smooth Intersection
// d1: signed distance to shape 1
// d2: signed distance to shape 2
// k: smoothness value for the trasition
float4 opIS(float4 d1, float4 d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2.w - d1.w) / k, 0.0, 1.0);
    float dist = lerp(d2.w, d1.w, h) + k * h * (1.0 - h);
    float3 color = lerp(d2.rgb, d1.rgb, h);

    return float4(color.rgb, dist);
}

// TRANSFORM FUNCTIONS //

float2x2 Rotate(float angle) {
    float s = sin(angle);
    float c = cos(angle);

    return float2x2(c, -s, s, c);
}

float3 R(float2 uv, float3 p, float3 l, float z) {
    float3 f = normalize(l - p),
         r = normalize(cross(float3(0, 1, 0), f)),
         u = cross(f, r),
         c = p + f * z,
         i = c + uv.x * r + uv.y * u,
         d = normalize(i - p);
    return d;
}

// --------------------------------------------------------------------------------------------//
float3 hsv2rgb(float3 c) {
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float map(float value, float min1, float max1, float min2, float max2) {
    return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

float sierpinski3(float3 z) {
    float2 mouse = iMouse.xy / iResolution.xy;
    float iterations = 25.0;
    float Scale = 2.0 + (sin(iTime / 2.0) + 1.0);
    float3 Offset = 3.0 * float3(1.0, 1.0, 1.0);
    float bailout = 1000.0;

    float r = length(z);
    int n = 0;
    while (n < int(iterations) && r < bailout) {

        z.x = abs(z.x);
        z.y = abs(z.y);
        z.z = abs(z.z);

        if (z.x - z.y < 0.0) z.xy = z.yx; // fold 1
        if (z.x - z.z < 0.0) z.xz = z.zx; // fold 2
        if (z.y - z.z < 0.0) z.zy = z.yz; // fold 3

        z.x = z.x * Scale - Offset.x * (Scale - 1.0);
        z.y = z.y * Scale - Offset.y * (Scale - 1.0);
        z.z = z.z * Scale;

        if (z.z > 0.5 * Offset.z * (Scale - 1.0)) {
            z.z -= Offset.z * (Scale - 1.0);
        }

        r = length(z);

        n++;
    }

    return (length(z) - 2.0) * pow(Scale, -float(n));
}

// Calculates de distance from a position p to the scene
float DistanceEstimator(float3 p) {
    p.yz = mul(Rotate(0.2 * PI), p.yz);
    p.yx = mul(Rotate(0.3 * PI), p.yx);
    p.xz = mul(Rotate(0.29 * PI), p.xz);
    float sierpinski = sierpinski3(p);
    return sierpinski;
}

// Marches the ray in the scene
float4 RayMarcher(float3 ro, float3 rd) {
    float steps = 0.0;
    float totalDistance = 0.0;
    float minDistToScene = 100.0;
    float3 minDistToScenePos = ro;
    float minDistToOrigin = 100.0;
    float3 minDistToOriginPos = ro;
    float4 col = float4(0.0, 0.0, 0.0, 1.0);
    float3 curPos = ro;
    bool hit = false;
    float3 p = float3(0.0, 0.0, 0.0);

    for (steps = 0.0; steps < float(MaximumRaySteps); steps++) {
        p = ro + totalDistance * rd;           // Current position of the ray
        float distance = DistanceEstimator(p); // Distance from the current position to the scene
        curPos = ro + rd * totalDistance;
        if (minDistToScene > distance) {
            minDistToScene = distance;
            minDistToScenePos = curPos;
        }
        if (minDistToOrigin > length(curPos)) {
            minDistToOrigin = length(curPos);
            minDistToOriginPos = curPos;
        }
        totalDistance += distance; // Increases the total distance armched
        if (distance < MinimumDistance) {
            hit = true;
            break; // If the ray marched more than the max steps or the max distance, breake out
        }
        else if (distance > MaximumDistance) {
            break;
        }
    }

    float iterations = float(steps) + log(log(MaximumDistance)) / log(2.0) - log(log(dot(curPos, curPos))) / log(2.0);

    if (minDistToScene > MinimumDistance) {
    }

    if (hit) {
        col.rgb = float3(0.8 + (length(curPos) / 8.0), 1.0, 0.8);
        col.rgb = hsv2rgb(col.rgb);
    }
    else {
        col.rgb = float3(0.8 + (length(minDistToScenePos) / 8.0), 1.0, 0.8);
        col.rgb = hsv2rgb(col.rgb);
        col.rgb *= 1.0 / pow(minDistToScene, 1.0);
        col.rgb /= 15.0 * map(sin(iTime * 3.0), -1.0, 1.0, 1.0, 3.0);
    }
    col.rgb /= iterations / 10.0; // Ambeint occlusion
    col.rgb /= pow(distance(ro, minDistToScenePos), 2.0);
    col.rgb *= 2000.0;

    return col;
}

void mainImage(out float4 fragColor, in float2 fragCoord) {
    // Normalized pixel coordinates (from 0 to 1)
    float2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    uv *= 0.2;
    uv.y -= 0.015;
    float2 m = iMouse.xy / iResolution.xy;

    float3 ro = float3(-40, 30.1, -10); // Ray origin
    // ro.yz *= Rotate (-m.y * 2.0 * PI + PI - 1.1); // Rotate thew ray with the mouse rotation
    ro.xz = mul(Rotate(-iTime * 2.0 * PI / 20.0), ro.xy);
    float3 rd = R(uv, ro, float3(0, 1, 0), 1.); // Ray direction (based on mouse rotation)

    float4 col = RayMarcher(ro, rd);

    // Output to screen
    fragColor = float4(col);
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 fragCoord = input.uv * iResolution;

    float4 outColor;
    mainImage(outColor, fragCoord);
    //outColor = float4(1,0,0,1);
    return outColor;
}