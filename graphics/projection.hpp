#pragma once


#include <limits>

#include "jms/external/glm.hpp"


namespace jms {


// Projection matrix:
// Right handed
// Y axis is down; Z axis is into the screen

// Depth range [1, inf]
glm::mat4 Perspective_RH_OI(float fovy, float aspect_ratio, float near);
// Depth range [1, 0]
glm::mat4 Perspective_RH_OZ(float fovy, float aspect_ratio, float near, float far);
// Depth range [0, inf]
glm::mat4 Perspective_RH_ZI(float fovy, float aspect_ratio, float near);
// Depth range [0, 1]
glm::mat4 Perspective_RH_ZO(float fovy, float aspect_ratio, float near, float far);


glm::mat4 Perspective_RH_OI(float fovy, float aspect_ratio, float near) {
    // Reference: Foundations of Game Engine Development by Eric Lengyel
    const float half_tan_fovy = glm::tan(fovy / 2.0f);
    glm::mat4 out{0.0f};
    out[0][0] = 1.0f / (half_tan_fovy * aspect_ratio);
    out[1][1] = 1.0f / half_tan_fovy;
    out[2][2] = std::numeric_limits<float>::epsilon();
    out[2][3] = 1.0f;
    out[3][2] = near * (1.0f - std::numeric_limits<float>::epsilon());
    return out;
}


glm::mat4 Perspective_RH_OZ(float fovy, float aspect_ratio, float near, float far) {
    // see notes below; swapping near and far also swaps the depth range
    return Perspective_RH_ZO(fovy, aspect_ratio, far, near);
}


glm::mat4 Perspective_RH_ZI(float fovy, float aspect_ratio, float near) {
    // Reference: Foundations of Game Engine Development by Eric Lengyel
    const float e = 1.0f - std::numeric_limits<float>::epsilon();
    const float half_tan_fovy = glm::tan(fovy / 2.0f);
    glm::mat4 out{0.0f};
    out[0][0] = 1.0f / (half_tan_fovy * aspect_ratio);
    out[1][1] = 1.0f / half_tan_fovy;
    out[2][2] = e;
    out[2][3] = 1.0f;
    out[3][2] = -near * e;
    return out;
}


glm::mat4 Perspective_RH_ZO(float fovy, float aspect_ratio, float near, float far) {
    const float half_tan_fovy = glm::tan(fovy / 2.0f);
    glm::mat4 out{0.0f};
    out[0][0] = 1.0f / (half_tan_fovy * aspect_ratio);
    out[1][1] = 1.0f / half_tan_fovy;
    out[2][2] = far / (far - near);
    out[2][3] = 1.0f;
    out[3][2] = -(far * near) / (far - near);
    return out;
}


// Reference: https://vincent-p.github.io/posts/vulkan_perspective_matrix/
// Reference: Foundations of Game Engine Development by Eric Lengyel
/*
    World
    Z Y
    |/__ X

    Frustrum
    ___ X
    |\
    Y Z
    CAMERA
    RIGHT - X
    FORWARD - Z
    UP - Y (pointing down)

    ________ f
 p->\ .    /          Z
 pn->\_.__/  n        |
        .    origin   .___x

    p  - point in frustrum
    pn - point projected on near plane

    x_pn   y_pn   z_pn     n
    ____ = ____ = _____ = ____
    x_p    y_p    z_p     z_p


    x_pn = (n * x_p) / z_p  =  (1 / z_p) * (n * x_p)
    y_pn = (n * y_p) / z_p  =  (1 / z_p) * (n * y_p)
    z_pn = (n * z_p) / z_p  =  (1 / z_p) * (n * z_p)

    w_c = z_p = 1 * z_p

       / .  .  .  . \     / x_p \     / x_c \
       | .  .  .  . |  *  | y_p |  =  | y_c |
       | .  .  .  . |     | z_p |     | z_c |
       \ 0, 0, 1, 0 /     \  1  /     \ w_c /


    Normalized device coordinates
       / x_n \     / x_c / w_c \
       | y_n |  =  | y_c / w_c |
       | z_n |     | z_c / w_c |
       \ w_n /     \ w_c / w_c /


    Near plane corners: l=left, t=top, r=right, b=bottom
    (l, t) = (-1,  1)
    (r, t) = ( 1,  1)
    (r, b) = ( 1, -1)
    (l, b) = (-1, -1)

    map near frustrum plane to near clip plane
    f(x) = mx + beta
    beta = f(x) - mx
    f(r) - mr = f(l) - ml
    f(r) - f(l) = mr - ml
    f(r) - f(l) = m(r - l)
    m = (f(r) - f(l)) / (r - l)
    m = (1 - (-1)) / (r - l)
*   m = 2 / (r - l)
    beta = f(r) - r(2 / (r - l))
    beta = 1 - (2r / (r - 1))
    beta = ((r - l) / (r - l)) - (2r / (r - l))
    beta = (r - l - 2r) / (r - l)
    beta = (-l - r) / (r - l)
*   beta = - (r + l) / (r - l)

    f(x_pn) = (2 / (r - l)) * x_pn - ((r + l) / (r - l))
    f(x_pn) = x_n
    x_n = (2 / (r - l)) * x_pn - ((r + l) / (r - l))

    f(y) = my + beta
    beta = f(y) - my
    f(t) - mt = f(b) - mb
    f(t) - f(b) = mt - mb
    m = (f(t) - f(b)) / (t - b)
    m = (1 - -1) / (t - b)
*   m = 2 / (t - b)
    beta = f(t) - t(2 / (t - b))
    beta = 1 - (2t / (t - b))
    beta = ((t - b) / (t - b)) - (2t / (t - b))
    beta = (t - b - 2t) / (t - b)
    beta = (-t - b) / (t - b)
*   beta = - (t + b) / (t - b)

    f(y_pn) = (2 / (t - b)) * y_pn - ((t + b) / (t - b))
    f(y_pn) = y_n
    y_n = (2 / (t - b)) * y_pn - ((t + b) / (t - b))

    Solve for x_n
    x_n = (2 / (r - l)) * x_pn - ((r + l) / (r - l))

          2*x_pn   (r + l)
    x_n = ______ - _______  ;  x_pn = (1 / z_p) * (n * x_p)
          (r - l)  (r - l)

             2n*x_p     (r + l)*z_p
    x_n = ___________ - ___________
          z_p*(r - l)   (r - l)*z_p

           1   /  2n          (r + l)     \
    x_n = ____ |_______ x_p - _______ z_p |
          z_p  \(r - l)       (r - l)     /


    Solve for y_n
    y_n = (2 / (t - b)) * y_pn - ((t + b) / (t - b))

          2*y_pn   (t + b)
    y_n = ______ - _______  ;  y_pn = (1 / z_p) * (n * y_p)
          (t - b)  (t - b)

             2n*y_p     (t + b)*z_p
    y_n = ___________ - ___________
          z_p*(t - b)   (t - b)*z_p

           1   /  2n          (t + b)     \
    y_n = ____ |_______ y_p - _______ z_p |
          z_p  \(t - b)       (t - b)     /


    Update matrix with x and y values
    *Note: z_p = w_c thus
    *Note: x_n = x_c / w_c
    *Note: y_n = y_c / w_c

       /  2n           (r+l)    \     /     \     /     \    /     \
       | _____    0    _____  0 |     | x_p |     | x_c |    | x_n |
       | (r-l)         (r-l)    |     |     |     |     |    |     |
       |                        |     |     |     |     |    |     |
       |         2n    (t+b)    |     |     |     |     |    |     |
       |   0    _____  _____  0 |  *  | y_p |  =  | y_c | -> | y_n |
       |        (t-b)  (t-b)    |     |     |     |     |    |     |
       |                        |     |     |     |     |    |     |
       |   0      0      A    B |     | z_p |     | z_c |    | z_n |
       |                        |     |     |     |     |    |     |
       \   0      0      1    0 /     \  1  /     \ w_c /    \  1  /

    z_c = ((A * z_p) + (B * w_p)) = (A * z_p) + B
    z_n = z_c / w_c = z_c / z_p
    z_n = ((A * z_p) + B) / z_p
    z_n = ((A * z_p) / z_p) + (B / z_p)
    z_n = A + (B / z_p)
    *NOTE: This is where the depth values can be switched [0, 1] or [1, 0] (reversed)
    if z_n = 1 then z_p = f
    if z_n = 0 then z_p = n
    1 = A + (B / f)
    0 = A + (B / n)

    0 = A + (B / n)
    -A = B / n
    B = -An

    1 = A + (B / f)
    1 = A + (-An / f)
    1 = A (1 - (n/f))
    1 = A (f/f - n/f)
    1 = A ((f - n)/f)
    A = f / (f - n)
    B = -An = -fn / (f - n)


       /  2n           (r+l)       \     /     \     /     \    /     \
       | _____    0    _____   0   |     | x_p |     | x_c |    | x_n |
       | (r-l)         (r-l)       |     |     |     |     |    |     |
       |                           |     |     |     |     |    |     |
       |         2n    (t+b)       |     |     |     |     |    |     |
       |   0    _____  _____   0   |     | y_p |     | y_c |    | y_n |
       |        (t-b)  (t-b)       |     |     |     |     |    |     |
       |                           |  *  |     |  =  |     | -> |     |
       |                 f    -fn  |     |     |     |     |    |     |
       |   0      0    _____ _____ |     | z_p |     | z_c |    | z_n |
       |               (f-n) (f-n) |     |     |     |     |    |     |
       |                           |     |     |     |     |    |     |
       \   0      0      1     0   /     \  1  /     \ w_c /    \  1  /


    *Note: l = -r and t = -b
           (r - l) = (r - (-r)) = 2r
           (r + l) = (r + (-r)) = 0
           (t - b) = (t - (-t)) = 2t
           (t + b) = (t + (-t)) = 0
    *Note: 2r = width and 2t = height
    *Note: tan0 = o / a = (height / 2) / n = height / 2n
    *Note: 2n / (t - b) = 2n / height = 1 / tan0
    *Note: aspect_ratio = width / height
           2n / (r - l)
           (2n / width) * aspect_ratio
           (2n / width) * (width / height)
           2n / height = 1 / tan0
    *Note: (1 / aspect_ratio) * (1 / tan0)
           (height / width) * (2n / height)
           2n / width
           2n / (r - l)

    s = aspect_ratio
    g = 1 / tan0

       /                           \     /     \     /     \    /     \
       |  g/s     0      0     0   |     | x_p |     | x_c |    | x_n |
       |                           |     |     |     |     |    |     |
       |   0      g      0     0   |     | y_p |     | y_c |    | y_n |
       |                           |     |     |     |     |    |     |
       |                 f    -fn  |  *  |     |  =  |     | -> |     |
       |   0      0    _____ _____ |     | z_p |     | z_c |    | z_n |
       |               (f-n) (f-n) |     |     |     |     |    |     |
       |                           |     |     |     |     |    |     |
       \   0      0      1     0   /     \  1  /     \ w_c /    \  1  /

*/

/*
 * Reverse depth buffer [1, 0]
 * Starting with above RH_ZO at the step where depth values can be switched


    *NOTE: This is where the depth values can be switched [0, 1] or [1, 0] (reversed)
    if z_n = 1 then z_p = n
    if z_n = 0 then z_p = f
    1 = A + (B / n)
    0 = A + (B / f)

    0 = A + (B / f)
    -A = B / f
    B = -Af

    1 = A + (B / n)
    1 = A + (-Af / n)
    1 = A (1 - (f/n))
    1 = A (n/n - f/n)
    1 = A ((n - f)/n)
    A = n / (n - f)
    B = -Af = -nf / (n - f)


       /  2n           (r+l)       \     /     \     /     \    /     \
       | _____    0    _____   0   |     | x_p |     | x_c |    | x_n |
       | (r-l)         (r-l)       |     |     |     |     |    |     |
       |                           |     |     |     |     |    |     |
       |         2n    (t+b)       |     |     |     |     |    |     |
       |   0    _____  _____   0   |     | y_p |     | y_c |    | y_n |
       |        (t-b)  (t-b)       |     |     |     |     |    |     |
       |                           |  *  |     |  =  |     | -> |     |
       |                 n    -nf  |     |     |     |     |    |     |
       |   0      0    _____ _____ |     | z_p |     | z_c |    | z_n |
       |               (n-f) (n-f) |     |     |     |     |    |     |
       |                           |     |     |     |     |    |     |
       \   0      0      1     0   /     \  1  /     \ w_c /    \  1  /


    *Note: l = -r and t = -b
           (r - l) = (r - (-r)) = 2r
           (r + l) = (r + (-r)) = 0
           (t - b) = (t - (-t)) = 2t
           (t + b) = (t + (-t)) = 0
    *Note: 2r = width and 2t = height
    *Note: tan0 = o / a = (height / 2) / n = height / 2n
    *Note: 2n / (t - b) = 2n / height = 1 / tan0
    *Note: aspect_ratio = width / height
           2n / (r - l)
           (2n / width) * aspect_ratio
           (2n / width) * (width / height)
           2n / height = 1 / tan0
    *Note: (1 / aspect_ratio) * (1 / tan0)
           (height / width) * (2n / height)
           2n / width
           2n / (r - l)

    s = aspect_ratio
    g = 1 / tan0

       /                           \     /     \     /     \    /     \
       |  g/s     0      0     0   |     | x_p |     | x_c |    | x_n |
       |                           |     |     |     |     |    |     |
       |   0      g      0     0   |     | y_p |     | y_c |    | y_n |
       |                           |     |     |     |     |    |     |
       |                 n    -nf  |  *  |     |  =  |     | -> |     |
       |   0      0    _____ _____ |     | z_p |     | z_c |    | z_n |
       |               (n-f) (n-f) |     |     |     |     |    |     |
       |                           |     |     |     |     |    |     |
       \   0      0      1     0   /     \  1  /     \ w_c /    \  1  /


*NOTE: switching near and far in Perspective_RH_ZO gives Perspective_RH_OZ and vice versa.
  n / (n - f)  ->  n=f and f=n  ->    f / (f - n)
-fn / (n - f)  ->  n=f and f=n  ->  -nf / (f - n)

*/


}
