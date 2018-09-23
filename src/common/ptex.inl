// This file is distributed under the MIT license.
// See the LICENSE file for details.

#include <cstddef>

#include <visionaray/math/simd/type_traits.h>
#include <visionaray/math/array.h>
#include <visionaray/math/forward.h>
#include <visionaray/math/vector.h>
#include <visionaray/texture/texture_traits.h>

namespace visionaray
{

//-------------------------------------------------------------------------------------------------
// Implement Visionaray's texturing interface for Ptex textures
//

namespace ptex
{

// tex2D
inline vector<4, unorm<8>> tex2D(PtexPtr<PtexTexture> const& tex, coordinate<float> const& coord)
{
    // Older versions of Ptex have only non-const PtexPtr accessors
    PtexPtr<Ptex::PtexTexture>& mutable_tex = const_cast<PtexPtr<PtexTexture>&>(tex);

    if (mutable_tex == nullptr)
    {
        return vector<4, unorm<8>>(1.0f);
    }

    Ptex::PtexFilter::Options opts(Ptex::PtexFilter::FilterType::f_bspline);
    PtexPtr<Ptex::PtexFilter> filter(Ptex::PtexFilter::getFilter(mutable_tex.get(), opts));

    int face_id = coord.face_id >= 0 ? coord.face_id : ~coord.face_id;
    auto face_data = mutable_tex->getData(face_id);
    auto res = face_data->res();

    vec3 rgb;
    filter->eval(
            rgb.data(),
            0,
            mutable_tex->numChannels(),
            face_id,
            coord.u,
            coord.v,
            1.0f / res.u(),
            0.0f,
            0.0f,
            1.0f / res.v()
            );

    return vector<4, unorm<8>>(rgb.x, rgb.y, rgb.z, 1.0f);
}

} // ptex


namespace simd
{

template <
    size_t N,
    typename T = float_from_simd_width<N>
    >
inline ptex::coordinate<T> pack(array<ptex::coordinate<float>, N> const& coords)
{
    ptex::coordinate<T> result;

    int* face_id = reinterpret_cast<int*>(&result.face_id);
    float* u = reinterpret_cast<float*>(&result.u);
    float* v = reinterpret_cast<float*>(&result.v);
    float* du = reinterpret_cast<float*>(&result.du);
    float* dv = reinterpret_cast<float*>(&result.dv);

    for (size_t i = 0; i < N; ++i)
    {
        face_id[i] = coords[i].face_id;
        u[i]       = coords[i].u;
        v[i]       = coords[i].v;
        du[i]      = coords[i].du;
        dv[i]      = coords[i].dv;
    }

    return result;
}

} // simd


// get_tex_coord() non-simd
template <
    typename HR,
    typename T,
    typename = typename std::enable_if<!simd::is_simd_vector<typename HR::scalar_type>::value>::type
    >
inline auto get_tex_coord(
        ptex::face_id_t const*  face_ids,
        HR const&               hr,
        basic_triangle<3, T>    /* */
        )
    -> ptex::coordinate<typename HR::scalar_type>
{
    ptex::coordinate<typename HR::scalar_type> result;

    vec2 tc1(0.0f, 0.0f);
    vec2 tc2(1.0f, 0.0f);
    vec2 tc3(1.0f, 1.0f);
    vec2 tc4(0.0f, 1.0f);

    result.face_id = face_ids[hr.prim_id];

    vec2 uv;

    if (result.face_id >= 0)
    {
        uv = lerp(tc1, tc2, tc3, hr.u, hr.v);
    }
    else
    {
        result.face_id = ~result.face_id;
        uv = lerp(tc1, tc3, tc4, hr.u, hr.v);
    }

    result.u = uv.x;
    result.v = uv.y;

    return result;
}

// get_tex_coord() simd
template <
    typename HR,
    typename T,
    typename = typename std::enable_if<simd::is_simd_vector<typename HR::scalar_type>::value>::type,
    typename = void
    >
inline auto get_tex_coord(
        ptex::face_id_t const*  face_ids,
        HR const&               hr,
        basic_triangle<3, T>    /* */
        )
    -> ptex::coordinate<typename HR::scalar_type>
{
    using U = typename HR::scalar_type;

    auto hrs = unpack(hr);

    array<ptex::coordinate<float>, simd::num_elements<U>::value> coords;

    for (int i = 0; i < simd::num_elements<U>::value; ++i)
    {
        coords[i].face_id = face_ids[hrs[i].prim_id];
        coords[i].u = hrs[i].u;
        coords[i].v = hrs[i].v;
    }

    return simd::pack(coords);
}

template <>
struct texture_dimensions<PtexPtr<PtexTexture>>
{
    enum { value = 2 };
};

} // visionaray
