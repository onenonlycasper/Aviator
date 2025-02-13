// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
 
// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../../include/fxcodec/fx_codec.h"
#include "codec_int.h"
#include "../fx_libopenjpeg/libopenjpeg20/openjpeg.h"
#include "../lcms2/include/fx_lcms2.h"
static void fx_error_callback(const char *msg, void *client_data)
{
    (void)client_data;
}
static void fx_warning_callback(const char *msg, void *client_data)
{
    (void)client_data;
}
static void fx_info_callback(const char *msg, void *client_data)
{
    (void)client_data;
}
typedef struct {
    const unsigned char* src_data;
    int					 src_size;
    int					 offset;
} decodeData;
static OPJ_SIZE_T opj_read_from_memory (void * p_buffer, OPJ_SIZE_T p_nb_bytes,  decodeData* srcData)
{
    if(srcData == NULL || srcData->src_size == 0 || srcData->src_data == NULL || srcData->offset >= srcData->src_size) {
        return -1;
    }
    OPJ_SIZE_T readlength = p_nb_bytes;
    OPJ_SIZE_T bufferLength = (OPJ_SIZE_T)(srcData->src_size - srcData->offset);
    if(bufferLength <= 0) {
        return 0;
    }
    if(bufferLength <= p_nb_bytes) {
        readlength = bufferLength;
    }
    memcpy(p_buffer, &(srcData->src_data[srcData->offset]), readlength);
    srcData->offset += (int)readlength;
    return readlength;
}
static OPJ_SIZE_T opj_write_from_memory (void * p_buffer, OPJ_SIZE_T p_nb_bytes, decodeData* srcData)
{
    if(srcData == NULL || srcData->src_size == 0 || srcData->src_data == NULL || srcData->offset >= srcData->src_size) {
        return -1;
    }
    OPJ_SIZE_T writeLength = p_nb_bytes;
    OPJ_SIZE_T bufferLength = (OPJ_SIZE_T)(srcData->src_size - srcData->offset);
    if(bufferLength <= p_nb_bytes) {
        writeLength = bufferLength;
    }
    memcpy((void*&)(srcData->src_data[srcData->offset]), p_buffer, writeLength);
    srcData->offset += (int)writeLength;
    return writeLength;
}
static OPJ_OFF_T opj_skip_from_memory (OPJ_OFF_T p_nb_bytes, decodeData* srcData)
{
    if(srcData == NULL || srcData->src_size == 0 || srcData->src_data == NULL || srcData->offset >= srcData->src_size) {
        return -1;
    }
    OPJ_OFF_T postion = srcData->offset + p_nb_bytes;
    if(postion < 0 ) {
        postion = 0;
    } else if (postion > srcData->src_size) {
    }
    srcData->offset = (int)postion;
    return p_nb_bytes;
}
static OPJ_BOOL opj_seek_from_memory (OPJ_OFF_T p_nb_bytes, decodeData * srcData)
{
    if(srcData == NULL || srcData->src_size == 0 || srcData->src_data == NULL || srcData->offset >= srcData->src_size) {
        return -1;
    }
    srcData->offset = (int)p_nb_bytes;
    if(srcData->offset < 0) {
        srcData->offset = 0;
    } else if(srcData->offset > srcData->src_size) {
        srcData->offset = srcData->src_size;
    }
    return OPJ_TRUE;
}
opj_stream_t* fx_opj_stream_create_memory_stream (decodeData* data,	OPJ_SIZE_T p_size, 	OPJ_BOOL p_is_read_stream)
{
    opj_stream_t* l_stream = 00;
    if (!data || ! data->src_data || data->src_size <= 0 ) {
        return NULL;
    }
    l_stream = opj_stream_create(p_size, p_is_read_stream);
    if (! l_stream) {
        return NULL;
    }
    opj_stream_set_user_data_v3(l_stream, data, NULL);
    opj_stream_set_user_data_length(l_stream, data->src_size);
    opj_stream_set_read_function(l_stream, (opj_stream_read_fn) opj_read_from_memory);
    opj_stream_set_write_function(l_stream, (opj_stream_write_fn) opj_write_from_memory);
    opj_stream_set_skip_function(l_stream, (opj_stream_skip_fn) opj_skip_from_memory);
    opj_stream_set_seek_function(l_stream, (opj_stream_seek_fn) opj_seek_from_memory);
    return l_stream;
}
static void sycc_to_rgb(int offset, int upb, int y, int cb, int cr,
                        int *out_r, int *out_g, int *out_b)
{
    int r, g, b;
    cb -= offset;
    cr -= offset;
    r = y + (int)(1.402 * (float)cr);
    if(r < 0) {
        r = 0;
    } else if(r > upb) {
        r = upb;
    } *out_r = r;
    g = y - (int)(0.344 * (float)cb + 0.714 * (float)cr);
    if(g < 0) {
        g = 0;
    } else if(g > upb) {
        g = upb;
    } *out_g = g;
    b = y + (int)(1.772 * (float)cb);
    if(b < 0) {
        b = 0;
    } else if(b > upb) {
        b = upb;
    } *out_b = b;
}
static void sycc444_to_rgb(opj_image_t *img)
{
    int *d0, *d1, *d2, *r, *g, *b;
    const int *y, *cb, *cr;
    int maxw, maxh, max, i, offset, upb;
    i = (int)img->comps[0].prec;
    offset = 1 << (i - 1);
    upb = (1 << i) - 1;
    maxw = (int)img->comps[0].w;
    maxh = (int)img->comps[0].h;
    max = maxw * maxh;
    y = img->comps[0].data;
    cb = img->comps[1].data;
    cr = img->comps[2].data;
    d0 = r = FX_Alloc(int, (size_t)max);
    d1 = g = FX_Alloc(int, (size_t)max);
    d2 = b = FX_Alloc(int, (size_t)max);
    for(i = 0; i < max; ++i) {
        sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
        ++y;
        ++cb;
        ++cr;
        ++r;
        ++g;
        ++b;
    }
    FX_Free(img->comps[0].data);
    img->comps[0].data = d0;
    FX_Free(img->comps[1].data);
    img->comps[1].data = d1;
    FX_Free(img->comps[2].data);
    img->comps[2].data = d2;
}
static void sycc422_to_rgb(opj_image_t *img)
{
    int *d0, *d1, *d2, *r, *g, *b;
    const int *y, *cb, *cr;
    int maxw, maxh, max, offset, upb;
    int i, j;
    i = (int)img->comps[0].prec;
    offset = 1 << (i - 1);
    upb = (1 << i) - 1;
    maxw = (int)img->comps[0].w;
    maxh = (int)img->comps[0].h;
    max = maxw * maxh;
    y = img->comps[0].data;
    cb = img->comps[1].data;
    cr = img->comps[2].data;
    d0 = r = FX_Alloc(int, (size_t)max);
    d1 = g = FX_Alloc(int, (size_t)max);
    d2 = b = FX_Alloc(int, (size_t)max);
    for(i = 0; i < maxh; ++i) {
        for(j = 0; j < maxw; j += 2) {
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            ++cb;
            ++cr;
        }
    }
    FX_Free(img->comps[0].data);
    img->comps[0].data = d0;
    FX_Free(img->comps[1].data);
    img->comps[1].data = d1;
    FX_Free(img->comps[2].data);
    img->comps[2].data = d2;
    img->comps[1].w = maxw;
    img->comps[1].h = maxh;
    img->comps[2].w = maxw;
    img->comps[2].h = maxh;
    img->comps[1].w = (OPJ_UINT32)maxw;
    img->comps[1].h = (OPJ_UINT32)maxh;
    img->comps[2].w = (OPJ_UINT32)maxw;
    img->comps[2].h = (OPJ_UINT32)maxh;
    img->comps[1].dx = img->comps[0].dx;
    img->comps[2].dx = img->comps[0].dx;
    img->comps[1].dy = img->comps[0].dy;
    img->comps[2].dy = img->comps[0].dy;
}
static void sycc420_to_rgb(opj_image_t *img)
{
    int *d0, *d1, *d2, *r, *g, *b, *nr, *ng, *nb;
    const int *y, *cb, *cr, *ny;
    int maxw, maxh, max, offset, upb;
    int i, j;
    i = (int)img->comps[0].prec;
    offset = 1 << (i - 1);
    upb = (1 << i) - 1;
    maxw = (int)img->comps[0].w;
    maxh = (int)img->comps[0].h;
    max = maxw * maxh;
    y = img->comps[0].data;
    cb = img->comps[1].data;
    cr = img->comps[2].data;
    d0 = r = FX_Alloc(int, (size_t)max);
    d1 = g = FX_Alloc(int, (size_t)max);
    d2 = b = FX_Alloc(int, (size_t)max);
    for(i = 0; i < maxh; i += 2) {
        ny = y + maxw;
        nr = r + maxw;
        ng = g + maxw;
        nb = b + maxw;
        for(j = 0; j < maxw;  j += 2) {
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            sycc_to_rgb(offset, upb, *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
            sycc_to_rgb(offset, upb, *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
            ++cb;
            ++cr;
        }
        y += maxw;
        r += maxw;
        g += maxw;
        b += maxw;
    }
    FX_Free(img->comps[0].data);
    img->comps[0].data = d0;
    FX_Free(img->comps[1].data);
    img->comps[1].data = d1;
    FX_Free(img->comps[2].data);
    img->comps[2].data = d2;
    img->comps[1].w = maxw;
    img->comps[1].h = maxh;
    img->comps[2].w = maxw;
    img->comps[2].h = maxh;
    img->comps[1].w = (OPJ_UINT32)maxw;
    img->comps[1].h = (OPJ_UINT32)maxh;
    img->comps[2].w = (OPJ_UINT32)maxw;
    img->comps[2].h = (OPJ_UINT32)maxh;
    img->comps[1].dx = img->comps[0].dx;
    img->comps[2].dx = img->comps[0].dx;
    img->comps[1].dy = img->comps[0].dy;
    img->comps[2].dy = img->comps[0].dy;
}
void color_sycc_to_rgb(opj_image_t *img)
{
    if(img->numcomps < 3) {
        img->color_space = OPJ_CLRSPC_GRAY;
        return;
    }
    if((img->comps[0].dx == 1)
            && (img->comps[1].dx == 2)
            && (img->comps[2].dx == 2)
            && (img->comps[0].dy == 1)
            && (img->comps[1].dy == 2)
            && (img->comps[2].dy == 2)) {
        sycc420_to_rgb(img);
    } else if((img->comps[0].dx == 1)
              && (img->comps[1].dx == 2)
              && (img->comps[2].dx == 2)
              && (img->comps[0].dy == 1)
              && (img->comps[1].dy == 1)
              && (img->comps[2].dy == 1)) {
        sycc422_to_rgb(img);
    } else if((img->comps[0].dx == 1)
              && (img->comps[1].dx == 1)
              && (img->comps[2].dx == 1)
              && (img->comps[0].dy == 1)
              && (img->comps[1].dy == 1)
              && (img->comps[2].dy == 1)) {
        sycc444_to_rgb(img);
    } else {
        return;
    }
    img->color_space = OPJ_CLRSPC_SRGB;
}
void color_apply_icc_profile(opj_image_t *image)
{
    cmsHPROFILE in_prof, out_prof;
    cmsHTRANSFORM transform;
    cmsColorSpaceSignature in_space, out_space;
    cmsUInt32Number intent, in_type, out_type, nr_samples;
    int *r, *g, *b;
    int prec, i, max, max_w, max_h;
    OPJ_COLOR_SPACE oldspace;
    in_prof =
        cmsOpenProfileFromMem(image->icc_profile_buf, image->icc_profile_len);
    if(in_prof == NULL) {
        return;
    }
    in_space = cmsGetPCS(in_prof);
    out_space = cmsGetColorSpace(in_prof);
    intent = cmsGetHeaderRenderingIntent(in_prof);
    max_w = (int)image->comps[0].w;
    max_h = (int)image->comps[0].h;
    prec = (int)image->comps[0].prec;
    oldspace = image->color_space;
    if(out_space == cmsSigRgbData) {
        if( prec <= 8 ) {
            in_type = TYPE_RGB_8;
            out_type = TYPE_RGB_8;
        } else {
            in_type = TYPE_RGB_16;
            out_type = TYPE_RGB_16;
        }
        out_prof = cmsCreate_sRGBProfile();
        image->color_space = OPJ_CLRSPC_SRGB;
    } else if(out_space == cmsSigGrayData) {
        if( prec <= 8 ) {
            in_type = TYPE_GRAY_8;
            out_type = TYPE_RGB_8;
        } else {
            in_type = TYPE_GRAY_16;
            out_type = TYPE_RGB_16;
        }
        out_prof = cmsCreate_sRGBProfile();
        image->color_space = OPJ_CLRSPC_SRGB;
    } else if(out_space == cmsSigYCbCrData) {
        in_type = TYPE_YCbCr_16;
        out_type = TYPE_RGB_16;
        out_prof = cmsCreate_sRGBProfile();
        image->color_space = OPJ_CLRSPC_SRGB;
    } else {
        return;
    }
    transform = cmsCreateTransform(in_prof, in_type,
                                   out_prof, out_type, intent, 0);
    cmsCloseProfile(in_prof);
    cmsCloseProfile(out_prof);
    if(transform == NULL) {
        image->color_space = oldspace;
        return;
    }
    if(image->numcomps > 2) {
        if( prec <= 8 ) {
            unsigned char *inbuf, *outbuf, *in, *out;
            max = max_w * max_h;
            nr_samples = (cmsUInt32Number)max * 3 * (cmsUInt32Number)sizeof(unsigned char);
            in = inbuf = FX_Alloc(unsigned char, nr_samples);
            out = outbuf = FX_Alloc(unsigned char, nr_samples);
            r = image->comps[0].data;
            g = image->comps[1].data;
            b = image->comps[2].data;
            for(i = 0; i < max; ++i) {
                *in++ = (unsigned char) * r++;
                *in++ = (unsigned char) * g++;
                *in++ = (unsigned char) * b++;
            }
            cmsDoTransform(transform, inbuf, outbuf, (cmsUInt32Number)max);
            r = image->comps[0].data;
            g = image->comps[1].data;
            b = image->comps[2].data;
            for(i = 0; i < max; ++i) {
                *r++ = (int) * out++;
                *g++ = (int) * out++;
                *b++ = (int) * out++;
            }
            FX_Free(inbuf);
            FX_Free(outbuf);
        } else {
            unsigned short *inbuf, *outbuf, *in, *out;
            max = max_w * max_h;
            nr_samples = (cmsUInt32Number)max * 3 * (cmsUInt32Number)sizeof(unsigned short);
            in = inbuf = FX_Alloc(unsigned short, nr_samples);
            out = outbuf = FX_Alloc(unsigned short, nr_samples);
            r = image->comps[0].data;
            g = image->comps[1].data;
            b = image->comps[2].data;
            for(i = 0; i < max; ++i) {
                *in++ = (unsigned short) * r++;
                *in++ = (unsigned short) * g++;
                *in++ = (unsigned short) * b++;
            }
            cmsDoTransform(transform, inbuf, outbuf, (cmsUInt32Number)max);
            r = image->comps[0].data;
            g = image->comps[1].data;
            b = image->comps[2].data;
            for(i = 0; i < max; ++i) {
                *r++ = (int) * out++;
                *g++ = (int) * out++;
                *b++ = (int) * out++;
            }
            FX_Free(inbuf);
            FX_Free(outbuf);
        }
    } else {
        unsigned char *in, *inbuf, *out, *outbuf;
        max = max_w * max_h;
        nr_samples = (cmsUInt32Number)max * 3 * sizeof(unsigned char);
        in = inbuf = FX_Alloc(unsigned char, nr_samples);
        out = outbuf = FX_Alloc(unsigned char, nr_samples);
        image->comps = (opj_image_comp_t*)
                       realloc(image->comps, (image->numcomps + 2) * sizeof(opj_image_comp_t));
        if(image->numcomps == 2) {
            image->comps[3] = image->comps[1];
        }
        image->comps[1] = image->comps[0];
        image->comps[2] = image->comps[0];
        image->comps[1].data = FX_Alloc(int, (size_t)max);
        FXSYS_memset8(image->comps[1].data, 0, sizeof(int) * (size_t)max);
        image->comps[2].data = FX_Alloc(int, (size_t)max);
        FXSYS_memset8(image->comps[2].data, 0, sizeof(int) * (size_t)max);
        image->numcomps += 2;
        r = image->comps[0].data;
        for(i = 0; i < max; ++i) {
            *in++ = (unsigned char) * r++;
        }
        cmsDoTransform(transform, inbuf, outbuf, (cmsUInt32Number)max);
        r = image->comps[0].data;
        g = image->comps[1].data;
        b = image->comps[2].data;
        for(i = 0; i < max; ++i) {
            *r++ = (int) * out++;
            *g++ = (int) * out++;
            *b++ = (int) * out++;
        }
        FX_Free(inbuf);
        FX_Free(outbuf);
    }
    cmsDeleteTransform(transform);
}
void color_apply_conversion(opj_image_t *image)
{
    int *row;
    int enumcs, numcomps;
    numcomps = image->numcomps;
    if(numcomps < 3) {
        return;
    }
    row = (int*)image->icc_profile_buf;
    enumcs = row[0];
    if(enumcs == 14) {
        int *L, *a, *b, *red, *green, *blue, *src0, *src1, *src2;
        double rl, ol, ra, oa, rb, ob, prec0, prec1, prec2;
        double minL, maxL, mina, maxa, minb, maxb;
        unsigned int default_type, il;
        unsigned int i, max, illu;
        cmsHPROFILE in, out;
        cmsHTRANSFORM transform;
        cmsUInt16Number RGB[3];
        cmsCIELab Lab;
        illu = 0;
        il = 0;
        in = cmsCreateLab4Profile(NULL);
        out = cmsCreate_sRGBProfile();
        transform =
            cmsCreateTransform(in, TYPE_Lab_DBL, out, TYPE_RGB_16,
                               INTENT_PERCEPTUAL, 0);
        cmsCloseProfile(in);
        cmsCloseProfile(out);
        if(transform == NULL) {
            return;
        }
        prec0 = (double)image->comps[0].prec;
        prec1 = (double)image->comps[1].prec;
        prec2 = (double)image->comps[2].prec;
        default_type = row[1];
        if(default_type == 0x44454600) {
            rl = 100;
            ra = 170;
            rb = 200;
            ol = 0;
            oa = pow(2, prec1 - 1);
            ob = pow(2, prec2 - 2)  + pow(2, prec2 - 3);
        } else {
            rl = row[2];
            ra = row[4];
            rb = row[6];
            ol = row[3];
            oa = row[5];
            ob = row[7];
        }
        L = src0 = image->comps[0].data;
        a = src1 = image->comps[1].data;
        b = src2 = image->comps[2].data;
        max = image->comps[0].w * image->comps[0].h;
        red = FX_Alloc(int, max);
        image->comps[0].data = red;
        green = FX_Alloc(int, max);
        image->comps[1].data = green;
        blue = FX_Alloc(int, max);
        image->comps[2].data = blue;
        minL = -(rl * ol) / (pow(2, prec0) - 1);
        maxL = minL + rl;
        mina = -(ra * oa) / (pow(2, prec1) - 1);
        maxa = mina + ra;
        minb = -(rb * ob) / (pow(2, prec2) - 1);
        maxb = minb + rb;
        for(i = 0; i < max; ++i) {
            Lab.L = minL + (double)(*L) * (maxL - minL) / (pow(2, prec0) - 1);
            ++L;
            Lab.a = mina + (double)(*a) * (maxa - mina) / (pow(2, prec1) - 1);
            ++a;
            Lab.b = minb + (double)(*b) * (maxb - minb) / (pow(2, prec2) - 1);
            ++b;
            cmsDoTransform(transform, &Lab, RGB, 1);
            *red++ = RGB[0];
            *green++ = RGB[1];
            *blue++ = RGB[2];
        }
        cmsDeleteTransform(transform);
        FX_Free(src0);
        FX_Free(src1);
        FX_Free(src2);
        image->color_space = OPJ_CLRSPC_SRGB;
        image->comps[0].prec = 16;
        image->comps[1].prec = 16;
        image->comps[2].prec = 16;
        return;
    }
}
class CJPX_Decoder : public CFX_Object
{
public:
    CJPX_Decoder();
    ~CJPX_Decoder();
    FX_BOOL	Init(const unsigned char* src_data, int src_size);
    void	GetInfo(FX_DWORD& width, FX_DWORD& height, FX_DWORD& codestream_nComps, FX_DWORD& output_nComps);
    FX_BOOL	Decode(FX_LPBYTE dest_buf, int pitch, FX_BOOL bTranslateColor, FX_LPBYTE offsets);
    FX_LPCBYTE m_SrcData;
    int m_SrcSize;
    opj_image_t *image;
    opj_codec_t* l_codec;
    opj_stream_t *l_stream;
    FX_BOOL m_useColorSpace;
};
CJPX_Decoder::CJPX_Decoder(): image(NULL), l_codec(NULL), l_stream(NULL), m_useColorSpace(FALSE)
{
}
CJPX_Decoder::~CJPX_Decoder()
{
    if(l_codec) {
        opj_destroy_codec(l_codec);
    }
    if(l_stream) {
        opj_stream_destroy(l_stream);
    }
    if(image) {
        opj_image_destroy(image);
    }
}
FX_BOOL CJPX_Decoder::Init(const unsigned char* src_data, int src_size)
{
    opj_dparameters_t parameters;
    try {
        image = NULL;
        m_SrcData = src_data;
        m_SrcSize = src_size;
        decodeData srcData;
        srcData.offset  = 0;
        srcData.src_size = src_size;
        srcData.src_data = src_data;
        l_stream = fx_opj_stream_create_memory_stream(&srcData, OPJ_J2K_STREAM_CHUNK_SIZE, 1);
        if (l_stream == NULL) {
            return FALSE;
        }
        opj_set_default_decoder_parameters(&parameters);
        parameters.decod_format = 0;
        parameters.cod_format = 3;
        if(FXSYS_memcmp32(m_SrcData, "\x00\x00\x00\x0c\x6a\x50\x20\x20\x0d\x0a\x87\x0a", 12) == 0) {
            l_codec = opj_create_decompress(OPJ_CODEC_JP2);
            parameters.decod_format = 1;
        } else {
            l_codec = opj_create_decompress(OPJ_CODEC_J2K);
        }
        if(!l_codec) {
            return FALSE;
        }
        opj_set_info_handler(l_codec, fx_info_callback, 00);
        opj_set_warning_handler(l_codec, fx_warning_callback, 00);
        opj_set_error_handler(l_codec, fx_error_callback, 00);
        if ( !opj_setup_decoder(l_codec, &parameters) ) {
            return FALSE;
        }
        if(! opj_read_header(l_stream, l_codec, &image)) {
            image = NULL;
            return FALSE;
        }
        if(this->m_useColorSpace) {
            image->useColorSpace = 1;
        } else {
            image->useColorSpace = 0;
        }
        if (!parameters.nb_tile_to_decode) {
            if (!opj_set_decode_area(l_codec, image, parameters.DA_x0,
                                     parameters.DA_y0, parameters.DA_x1, parameters.DA_y1)) {
                opj_image_destroy(image);
                image = NULL;
                return FALSE;
            }
            if (!(opj_decode(l_codec, l_stream, image) && opj_end_decompress(l_codec,	l_stream))) {
                opj_image_destroy(image);
                image = NULL;
                return FALSE;
            }
        } else {
            if (!opj_get_decoded_tile(l_codec, l_stream, image, parameters.tile_index)) {
                return FALSE;
            }
        }
        opj_stream_destroy(l_stream);
        l_stream = NULL;
        if( image->color_space != OPJ_CLRSPC_SYCC
                && image->numcomps == 3 && image->comps[0].dx == image->comps[0].dy
                && image->comps[1].dx != 1 ) {
            image->color_space = OPJ_CLRSPC_SYCC;
        } else if (image->numcomps <= 2) {
            image->color_space = OPJ_CLRSPC_GRAY;
        }
        if(image->color_space == OPJ_CLRSPC_SYCC) {
            color_sycc_to_rgb(image);
        }
        if(image->icc_profile_buf && !image->useColorSpace) {
            FX_Free(image->icc_profile_buf);
            image->icc_profile_buf = NULL;
            image->icc_profile_len = 0;
        }
        if(!image) {
            return FALSE;
        }
    } catch (...) {
        return FALSE;
    }
    return TRUE;
}
void CJPX_Decoder::GetInfo(FX_DWORD& width, FX_DWORD& height, FX_DWORD& codestream_nComps, FX_DWORD& output_nComps)
{
    width = (FX_DWORD)image->x1;
    height = (FX_DWORD)image->y1;
    output_nComps = codestream_nComps = (FX_DWORD)image->numcomps;
}
FX_BOOL CJPX_Decoder::Decode(FX_LPBYTE dest_buf, int pitch, FX_BOOL bTranslateColor, FX_LPBYTE offsets)
{
    FX_BYTE** channel_bufs;
    int* adjust_comps;
    int i, wid, hei, row, col, channel, src;
    FX_BOOL flag;
    FX_LPBYTE pChannel, pScanline, pPixel;
    try {
        if(image->comps[0].w != image->x1 || image->comps[0].h != image->y1) {
            return FALSE;
        }
        if(pitch < (int)(image->comps[0].w * 8 * image->numcomps + 31) >> 5 << 2) {
            return FALSE;
        }
        FXSYS_memset8(dest_buf, 0xff, image->y1 * pitch);
        channel_bufs = FX_Alloc(FX_BYTE*, image->numcomps);
        if (channel_bufs == NULL) {
            return FALSE;
        }
        adjust_comps = FX_Alloc(int, image->numcomps);
        if (adjust_comps == NULL) {
            FX_Free(channel_bufs);
            return FALSE;
        }
        flag = TRUE;
        for (i = 0; i < (int)image->numcomps; i ++) {
            channel_bufs[i] = dest_buf + offsets[i];
            adjust_comps[i] = image->comps[i].prec - 8;
            if(i > 0) {
                if(image->comps[i].dx != image->comps[i - 1].dx
                        || image->comps[i].dy != image->comps[i - 1].dy
                        || image->comps[i].prec != image->comps[i - 1].prec) {
                    flag = FALSE;
                    goto failed;
                }
            }
        }
        wid = image->comps[0].w;
        hei = image->comps[0].h;
        for (channel = 0; channel < (int)image->numcomps; channel++) {
            pChannel = channel_bufs[channel];
            if(adjust_comps[channel] < 0) {
                for(row = 0; row < hei; row++) {
                    pScanline = pChannel + row * pitch;
                    for (col = 0; col < wid; col++) {
                        pPixel = pScanline + col * image->numcomps;
                        src = image->comps[channel].data[row * wid + col];
                        src += image->comps[channel].sgnd ? 1 << (image->comps[channel].prec - 1) : 0;
                        if (adjust_comps[channel] > 0) {
                            *pPixel = 0;
                        } else {
                            *pPixel = (FX_BYTE)(src << -adjust_comps[channel]);
                        }
                    }
                }
            } else {
                for(row = 0; row < hei; row++) {
                    pScanline = pChannel + row * pitch;
                    for (col = 0; col < wid; col++) {
                        pPixel = pScanline + col * image->numcomps;
                        if (!image->comps[channel].data) {
                            continue;
                        }
                        src = image->comps[channel].data[row * wid + col];
                        src += image->comps[channel].sgnd ? 1 << (image->comps[channel].prec - 1) : 0;
                        if (adjust_comps[channel] - 1 < 0) {
                            *pPixel = (FX_BYTE)((src >> adjust_comps[channel]));
                        } else {
                            int tmpPixel = (src >> adjust_comps[channel]) + ((src >> (adjust_comps[channel] - 1)) % 2);
                            if (tmpPixel > 255) {
                                tmpPixel = 255;
                            } else if (tmpPixel < 0) {
                                tmpPixel = 0;
                            }
                            *pPixel = (FX_BYTE)tmpPixel;
                        }
                    }
                }
            }
        }
    } catch (...) {
        if (channel_bufs) {
            FX_Free(channel_bufs);
        }
        FX_Free(adjust_comps);
        return FALSE;
    }
    FX_Free(channel_bufs);
    FX_Free(adjust_comps);
    return TRUE;
failed:
    FX_Free(channel_bufs);
    FX_Free(adjust_comps);
    return FALSE;
}
void initialize_transition_table();
void initialize_significance_luts();
void initialize_sign_lut();
CCodec_JpxModule::CCodec_JpxModule()
{
}
void* CCodec_JpxModule::CreateDecoder(FX_LPCBYTE src_buf, FX_DWORD src_size , FX_BOOL useColorSpace)
{
    CJPX_Decoder* pDecoder = FX_NEW CJPX_Decoder;
    if (pDecoder == NULL) {
        return NULL;
    }
    pDecoder->m_useColorSpace = useColorSpace;
    if (!pDecoder->Init(src_buf, src_size)) {
        delete pDecoder;
        return NULL;
    }
    return pDecoder;
}
void CCodec_JpxModule::GetImageInfo(FX_LPVOID ctx, FX_DWORD& width, FX_DWORD& height,
                                    FX_DWORD& codestream_nComps, FX_DWORD& output_nComps)
{
    CJPX_Decoder* pDecoder = (CJPX_Decoder*)ctx;
    pDecoder->GetInfo(width, height, codestream_nComps, output_nComps);
}
FX_BOOL CCodec_JpxModule::Decode(void* ctx, FX_LPBYTE dest_data, int pitch, FX_BOOL bTranslateColor, FX_LPBYTE offsets)
{
    CJPX_Decoder* pDecoder = (CJPX_Decoder*)ctx;
    return pDecoder->Decode(dest_data, pitch, bTranslateColor, offsets);
}
void CCodec_JpxModule::DestroyDecoder(void* ctx)
{
    CJPX_Decoder* pDecoder = (CJPX_Decoder*)ctx;
    delete pDecoder;
}
