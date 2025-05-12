#version 440

// TODO: Provide no AA version, when the build system starts supporting defines.
#define AA 3

/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * The MIT License
 * Copyright (C) 2015 Inigo Quilez
 * Copyright (C) 2019 Inigo Quilez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *****************************************************************************/

// Provided by `ShaderEffect`'s default vertex shader even though there is no texture:
layout(location = 0) in vec2 qt_TexCoord0;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;

    float time; // seed
    vec2 size;
    vec4 color; // alpha is not respected, but qt_Opacity is respected
};

// https://iquilezles.org/articles/distfunctions
float sdCappedCone(vec3 p, vec3 a, vec3 b, float ra, float rb)
{
    float rba  = rb-ra;
    float baba = dot(b-a,b-a);
    float papa = dot(p-a,p-a);
    float paba = dot(p-a,b-a)/baba;

    float x = sqrt( papa - paba*paba*baba );

    float cax = max(0.0,x-((paba<0.5)?ra:rb));
    float cay = abs(paba-0.5)-0.5;

    float k = rba*rba + baba;
    float f = clamp( (rba*(x-ra)+paba*baba)/k, 0.0, 1.0 );

    float cbx = x-ra - f*rba;
    float cby = paba - f;

    float s = (cbx < 0.0 && cay < 0.0) ? -1.0 : 1.0;

    return s*sqrt( min(cax*cax + cay*cay*baba,
                       cbx*cbx + cby*cby*baba) );
}

// https://iquilezles.org/articles/distfunctions
float opSmoothUnion( float d1, float d2, float k )
{
    float h = clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return mix( d2, d1, h ) - k*h*(1.0-h);
}

// https://iquilezles.org/articles/distfunctions
float sdRoundBox( vec3 p, vec3 b, float r )
{
  vec3 q = abs(p) - b + r;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0) - r;
}

float map( in vec3 pos )
{
    // Centered VLC Cone:

    // Base / platform:
    const float baseY = 0.02; // height
    const float baseXZ = 0.35; // base size, symmetric
    const float baseRounding = 0.01;
    const float distRoundBox = sdRoundBox(vec3(pos.x, pos.y + baseY, pos.z), vec3(baseXZ, baseY, baseXZ), baseRounding);
    // Cone (capped):
    const float coneY = 0.7; // height
    const float coneRA = 0.25; // bottom radius
    const float coneRB = 0.05; // top radius
    const float distCone = sdCappedCone(pos, vec3(0.0, 0.0, 0.0), vec3(0.0, coneY, 0.0), coneRA, coneRB);

    // Smooth union of the base and the cone:
    const float unionSmoothness = 0.01;
    return opSmoothUnion(distRoundBox, distCone, unionSmoothness);
}

// https://iquilezles.org/articles/normalsSDF
vec3 calcNormal( in vec3 pos )
{
    vec2 e = vec2(1.0,-1.0)*0.5773;
    const float eps = 0.0005;
    return normalize( e.xyy*map( pos + e.xyy*eps ) +
                      e.yyx*map( pos + e.yyx*eps ) +
                      e.yxy*map( pos + e.yxy*eps ) +
                      e.xxx*map( pos + e.xxx*eps ) );
}

// Raymarching (https://iquilezles.org/articles/raymarchingdf):
// - https://www.shadertoy.com/view/Xds3zN
// - https://www.shadertoy.com/view/tsSXzK
/// <raymarching>
mat3 setCamera( in vec3 ro, in vec3 ta, float cr )
{
    vec3 cw = normalize(ta-ro);
    vec3 cp = vec3(sin(cr), cos(cr),0.0);
    vec3 cu = normalize( cross(cw,cp) );
    vec3 cv =          ( cross(cu,cw) );
    return mat3( cu, cv, cw );
}

vec4 render(vec2 pos)
{
    vec2 mo = vec2(1.0, 1.0)/size.xy;
    float time = 32.0 + time*10.5;

    // camera
    vec3 ta = vec3( 0.0, 0.25, 0.0 );
    vec3 ro = ta + vec3( 4.5*cos(0.1*time + 7.0*mo.x), 2.0, 4.5*sin(0.1*time + 7.0*mo.x) );
    // camera-to-world transformation
    mat3 ca = setCamera( ro, ta, 0.0 );

    vec4 tot = vec4(0.0);

    #if AA>1
    for( int m=0; m<AA; m++ )
    for( int n=0; n<AA; n++ )
    {
        // pixel coordinates
        vec2 o = vec2(float(m),float(n)) / float(AA) - 0.;
        vec2 p = (-size.xy + 2.0*(pos+o))/size.y;
        #else
        vec2 p = (-size.xy + 2.0*pos)/size.y;
        #endif

        // focal length
        const float fl = 9.5;

        // ray direction
        vec3 rd = ca * normalize( vec3(p,fl) );

        // raymarch
        const float tmax = 6.0;
        float t = 0.0;
        for( int i=0; i<256; i++ )
        {
            vec3 pos = ro + t*rd;
            float h = map(pos);
            if( h<0.0001 || t>tmax ) break;
            t += h;
        }

        // shading/lighting
        vec3 col = vec3(0.0);
        if( t<tmax )
        {
            vec3 pos = ro + t*rd;
            vec3 nor = calcNormal(pos);
            float dif = clamp( dot(nor,vec3(0.57703)), 0.0, 1.0);
            float amb = 0.5 + 0.5*dot(nor,vec3(0.0,1.0,0.0));
            col = ((pos.y > 0.15 && pos.y < 0.3) || (pos.y > 0.45 && pos.y < 0.6) ? vec3(1.,1.,1.) : color.rgb)*amb + vec3(0.8,0.7,0.5)*dif * 0.25;
            tot += vec4(col, 1.0);
        }
    #if AA>1
    }
    tot /= float(AA*AA);
    #endif

    return tot;
}
/// </raymarching>

void main()
{
    vec2 mapped = qt_TexCoord0 * size;
    mapped = vec2(mapped.x, size - mapped.y); // inversion

    fragColor = render(mapped) * qt_Opacity;
}
