/* libs/graphics/sgl/SkSpriteBlitter_RGB16.cpp
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License"); 
** you may not use this file except in compliance with the License. 
** You may obtain a copy of the License at 
**
**     http://www.apache.org/licenses/LICENSE-2.0 
**
** Unless required by applicable law or agreed to in writing, software 
** distributed under the License is distributed on an "AS IS" BASIS, 
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
** See the License for the specific language governing permissions and 
** limitations under the License.
*/

#include "SkSpriteBlitter.h"
#include "SkBlitRow.h"
#include "SkTemplates.h"
#include "SkUtils.h"
#include "SkColorPriv.h"

#ifdef BLTSVILLE_ENHANCEMENT
#include <bltsville.h>
extern void* hbvlib;
extern BVFN_MAP bv_map;
extern BVFN_BLT bv_blt;
extern BVFN_UNMAP bv_unmap;
#endif

#define D16_S32A_Opaque_Pixel(dst, sc)                                        \
do {                                                                          \
    if (sc) {                                                                 \
        *dst = SkSrcOver32To16(sc, *dst);                                     \
    }                                                                         \
} while (0)

static inline void D16_S32A_Blend_Pixel_helper(uint16_t* dst, SkPMColor sc,
                                               unsigned src_scale) {
    uint16_t dc = *dst;
    unsigned sa = SkGetPackedA32(sc);
    unsigned dr, dg, db;

    if (255 == sa) {
        dr = SkAlphaBlend(SkPacked32ToR16(sc), SkGetPackedR16(dc), src_scale);
        dg = SkAlphaBlend(SkPacked32ToG16(sc), SkGetPackedG16(dc), src_scale);
        db = SkAlphaBlend(SkPacked32ToB16(sc), SkGetPackedB16(dc), src_scale);
    } else {
        unsigned dst_scale = 255 - SkAlphaMul(sa, src_scale);
        dr = (SkPacked32ToR16(sc) * src_scale +
              SkGetPackedR16(dc) * dst_scale) >> 8;
        dg = (SkPacked32ToG16(sc) * src_scale +
              SkGetPackedG16(dc) * dst_scale) >> 8;
        db = (SkPacked32ToB16(sc) * src_scale +
              SkGetPackedB16(dc) * dst_scale) >> 8;
    }
    *dst = SkPackRGB16(dr, dg, db);
}

#define D16_S32A_Blend_Pixel(dst, sc, src_scale) \
    do { if (sc) D16_S32A_Blend_Pixel_helper(dst, sc, src_scale); } while (0)


///////////////////////////////////////////////////////////////////////////////

class Sprite_D16_S16_Opaque : public SkSpriteBlitter {
public:
    Sprite_D16_S16_Opaque(const SkBitmap& source)
        : SkSpriteBlitter(source) {}

    // overrides
    virtual void blitRect(int x, int y, int width, int height) {
#ifdef BLTSVILLE_ENHANCEMENT
        enum bverror bverr = BVERR_UNK;

        if(hbvlib) {
            struct bvbltparams params;
            struct bvbuffdesc srcdesc, dstdesc;
            struct bvsurfgeom srcgeom, dstgeom;

            memset(&params, 0, sizeof(params));
            params.structsize = sizeof(params);

            params.src1.desc = &srcdesc;
            params.src1geom = &srcgeom;

            params.dstdesc = &dstdesc;
            params.dstgeom = &dstgeom;

            memset(&srcgeom, 0, sizeof(srcgeom));
            srcgeom.structsize = sizeof(srcgeom);
            srcgeom.format = OCDFMT_RGB16;

            memset(&srcdesc, 0, sizeof(srcdesc));
            srcdesc.structsize = sizeof(srcdesc);

            memset(&dstgeom, 0, sizeof(dstgeom));
            dstgeom.structsize = sizeof(dstgeom);
            dstgeom.format = OCDFMT_RGB16;

            memset(&dstdesc, 0, sizeof(dstdesc));
            dstdesc.structsize = sizeof(dstdesc);

            params.src1rect.left = x - fLeft;
            params.src1rect.top = y - fTop;
            params.src1rect.width = width;
            params.src1rect.height = height;

            params.dstrect.left = x;
            params.dstrect.top = y;
            params.dstrect.width = width;
            params.dstrect.height = height;

            dstgeom.width = fDevice->width();
            dstgeom.height = fDevice->height();
            dstgeom.virtstride = fDevice->rowBytes();

            srcgeom.width = fSource->width();
            srcgeom.height = fSource->height();
            srcgeom.virtstride = fSource->rowBytes();

            srcdesc.virtaddr = fSource->getPixels();
            srcdesc.length = srcgeom.virtstride * srcgeom.height;

            dstdesc.virtaddr = fDevice->getPixels();
            dstdesc.length = dstgeom.virtstride * dstgeom.height;
            if(srcdesc.virtaddr == 0 || dstdesc.virtaddr == 0) {
                goto SKIA_D16_S16;
            }

            SkXfermode* xfer = fPaint->getXfermode();
            int alpha = fPaint->getAlpha();
            int operation = 0;

            SkXfermode::Mode mode;
            bool getMode = SkXfermode::IsMode(xfer, &mode);

            if (SkXfermode::kSrcOver_Mode == mode) {
                if (0 == alpha) {
                    mode = SkXfermode::kDst_Mode;
                } else if (0xFF == alpha && fSource->isOpaque()) {
                    mode = SkXfermode::kSrc_Mode;
                }
            }

            if (fPaint->getColorFilter() != NULL ||
                fPaint->getTypeface() != NULL ||
                fPaint->getShader() != NULL ||
                fPaint->getMaskFilter() != NULL ||
                fPaint->getLooper() != NULL) {
                goto SKIA_D16_S16;
            }

            switch(mode) {

                case SkXfermode::kSrc_Mode:
                    if(alpha != 255) {
                        params.flags = BVFLAG_BLEND;
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1 + BVBLENDDEF_GLOBAL_UCHAR);
                        params.globalalpha.size8 = alpha;
                    } else {
                        params.flags = BVFLAG_ROP;
                        params.op.rop = 0xCCCC;
                    }

                    break;

                case SkXfermode::kSrcOver_Mode:
                    if(alpha == 255 && fSource->isOpaque()) {
                        operation = 1; //ROP 0xCCCC -dst RGB1|src RGBx
                        params.flags = BVFLAG_ROP;
                        params.op.rop = 0xCCCC;
                    } else if(alpha == 255 && fDevice->isOpaque()) {
                        operation = 2; //BLEND -dst RGB1|src RGBA
                        params.flags = BVFLAG_BLEND;
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER);
                    } else if(alpha == 255) {
                        operation = 3; //BLEND -dst RGBA|src RGBA
                        params.flags = BVFLAG_BLEND;
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER);
                    } else {
                        params.flags = BVFLAG_BLEND;
                        operation = 4;
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                        params.globalalpha.size8 = alpha;
                    }

                    break;

                default:
                    goto SKIA_D16_S16;
                    break;
            }
            bverr = bv_blt(&params);
        }
SKIA_D16_S16:
        if(bverr != BVERR_NONE) {
#endif
        SK_RESTRICT uint16_t* dst = fDevice->getAddr16(x, y);
        const SK_RESTRICT uint16_t* src = fSource->getAddr16(x - fLeft,
                                                             y - fTop);
        unsigned dstRB = fDevice->rowBytes();
        unsigned srcRB = fSource->rowBytes();

        while (--height >= 0) {
            memcpy(dst, src, width << 1);
            dst = (uint16_t*)((char*)dst + dstRB);
            src = (const uint16_t*)((const char*)src + srcRB);
        }
#ifdef BLTSVILLE_ENHANCEMENT
        }
#endif
    }
};

#define D16_S16_Blend_Pixel(dst, sc, scale)     \
    do {                                        \
        uint16_t dc = *dst;                     \
        *dst = SkBlendRGB16(sc, dc, scale);     \
    } while (0)

#define SkSPRITE_CLASSNAME                  Sprite_D16_S16_Blend
#define SkSPRITE_ARGS                       , uint8_t alpha
#define SkSPRITE_FIELDS                     uint8_t  fSrcAlpha;
#define SkSPRITE_INIT                       fSrcAlpha = alpha;
#define SkSPRITE_DST_TYPE                   uint16_t
#define SkSPRITE_SRC_TYPE                   uint16_t
#define SkSPRITE_DST_GETADDR                getAddr16
#define SkSPRITE_SRC_GETADDR                getAddr16
#define SkSPRITE_PREAMBLE(srcBM, x, y)      int scale = SkAlpha255To256(fSrcAlpha);
#define SkSPRITE_BLIT_PIXEL(dst, src)       D16_S16_Blend_Pixel(dst, src, scale)
#define SkSPRITE_NEXT_ROW
#define SkSPRITE_POSTAMBLE(srcBM)
#include "SkSpriteBlitterTemplate.h"

///////////////////////////////////////////////////////////////////////////////

#define D16_S4444_Opaque(dst, sc)           \
    do {                                    \
        uint16_t dc = *dst;                 \
        *dst = SkSrcOver4444To16(sc, dc);   \
    } while (0)

#define SkSPRITE_CLASSNAME                  Sprite_D16_S4444_Opaque
#define SkSPRITE_ARGS                       
#define SkSPRITE_FIELDS                     
#define SkSPRITE_INIT                       
#define SkSPRITE_DST_TYPE                   uint16_t
#define SkSPRITE_SRC_TYPE                   SkPMColor16
#define SkSPRITE_DST_GETADDR                getAddr16
#define SkSPRITE_SRC_GETADDR                getAddr16
#define SkSPRITE_PREAMBLE(srcBM, x, y)      
#define SkSPRITE_BLIT_PIXEL(dst, src)       D16_S4444_Opaque(dst, src)
#define SkSPRITE_NEXT_ROW
#define SkSPRITE_POSTAMBLE(srcBM)
#include "SkSpriteBlitterTemplate.h"

#define D16_S4444_Blend(dst, sc, scale16)           \
    do {                                            \
        uint16_t dc = *dst;                         \
        *dst = SkBlend4444To16(sc, dc, scale16);    \
    } while (0)


#define SkSPRITE_CLASSNAME                  Sprite_D16_S4444_Blend
#define SkSPRITE_ARGS                       , uint8_t alpha
#define SkSPRITE_FIELDS                     uint8_t  fSrcAlpha;
#define SkSPRITE_INIT                       fSrcAlpha = alpha;
#define SkSPRITE_DST_TYPE                   uint16_t
#define SkSPRITE_SRC_TYPE                   uint16_t
#define SkSPRITE_DST_GETADDR                getAddr16
#define SkSPRITE_SRC_GETADDR                getAddr16
#define SkSPRITE_PREAMBLE(srcBM, x, y)      int scale = SkAlpha15To16(fSrcAlpha);
#define SkSPRITE_BLIT_PIXEL(dst, src)       D16_S4444_Blend(dst, src, scale)
#define SkSPRITE_NEXT_ROW
#define SkSPRITE_POSTAMBLE(srcBM)
#include "SkSpriteBlitterTemplate.h"

///////////////////////////////////////////////////////////////////////////////

#define SkSPRITE_CLASSNAME                  Sprite_D16_SIndex8A_Opaque
#define SkSPRITE_ARGS
#define SkSPRITE_FIELDS
#define SkSPRITE_INIT
#define SkSPRITE_DST_TYPE                   uint16_t
#define SkSPRITE_SRC_TYPE                   uint8_t
#define SkSPRITE_DST_GETADDR                getAddr16
#define SkSPRITE_SRC_GETADDR                getAddr8
#define SkSPRITE_PREAMBLE(srcBM, x, y)      const SkPMColor* ctable = srcBM.getColorTable()->lockColors()
#define SkSPRITE_BLIT_PIXEL(dst, src)       D16_S32A_Opaque_Pixel(dst, ctable[src])
#define SkSPRITE_NEXT_ROW
#define SkSPRITE_POSTAMBLE(srcBM)           srcBM.getColorTable()->unlockColors(false)
#include "SkSpriteBlitterTemplate.h"

#define SkSPRITE_CLASSNAME                  Sprite_D16_SIndex8A_Blend
#define SkSPRITE_ARGS                       , uint8_t alpha
#define SkSPRITE_FIELDS                     uint8_t fSrcAlpha;
#define SkSPRITE_INIT                       fSrcAlpha = alpha;
#define SkSPRITE_DST_TYPE                   uint16_t
#define SkSPRITE_SRC_TYPE                   uint8_t
#define SkSPRITE_DST_GETADDR                getAddr16
#define SkSPRITE_SRC_GETADDR                getAddr8
#define SkSPRITE_PREAMBLE(srcBM, x, y)      const SkPMColor* ctable = srcBM.getColorTable()->lockColors(); unsigned src_scale = SkAlpha255To256(fSrcAlpha);
#define SkSPRITE_BLIT_PIXEL(dst, src)       D16_S32A_Blend_Pixel(dst, ctable[src], src_scale)
#define SkSPRITE_NEXT_ROW
#define SkSPRITE_POSTAMBLE(srcBM)           srcBM.getColorTable()->unlockColors(false);
#include "SkSpriteBlitterTemplate.h"

///////////////////////////////////////////////////////////////////////////////

static intptr_t asint(const void* ptr) {
    return reinterpret_cast<const char*>(ptr) - (const char*)0;
}

static void blitrow_d16_si8(SK_RESTRICT uint16_t* dst,
                            SK_RESTRICT const uint8_t* src, int count,
                            SK_RESTRICT const uint16_t* ctable) {
    if (count <= 8) {
        do {
            *dst++ = ctable[*src++];
        } while (--count);
        return;
    }

    // eat src until we're on a 4byte boundary
    while (asint(src) & 3) {
        *dst++ = ctable[*src++];
        count -= 1;
    }

    int qcount = count >> 2;
    SkASSERT(qcount > 0);
    const uint32_t* qsrc = reinterpret_cast<const uint32_t*>(src);
    if (asint(dst) & 2) {
        do {
            uint32_t s4 = *qsrc++;
#ifdef SK_CPU_LENDIAN
            *dst++ = ctable[s4 & 0xFF];
            *dst++ = ctable[(s4 >> 8) & 0xFF];
            *dst++ = ctable[(s4 >> 16) & 0xFF];
            *dst++ = ctable[s4 >> 24];
#else   // BENDIAN
            *dst++ = ctable[s4 >> 24];
            *dst++ = ctable[(s4 >> 16) & 0xFF];
            *dst++ = ctable[(s4 >> 8) & 0xFF];
            *dst++ = ctable[s4 & 0xFF];
#endif
        } while (--qcount);
    } else {    // dst is on a 4byte boundary
        uint32_t* ddst = reinterpret_cast<uint32_t*>(dst);
        do {
            uint32_t s4 = *qsrc++;
#ifdef SK_CPU_LENDIAN
            *ddst++ = (ctable[(s4 >> 8) & 0xFF] << 16) | ctable[s4 & 0xFF];
            *ddst++ = (ctable[s4 >> 24] << 16) | ctable[(s4 >> 16) & 0xFF];
#else   // BENDIAN
            *ddst++ = (ctable[s4 >> 24] << 16) | ctable[(s4 >> 16) & 0xFF];
            *ddst++ = (ctable[(s4 >> 8) & 0xFF] << 16) | ctable[s4 & 0xFF];
#endif
        } while (--qcount);
        dst = reinterpret_cast<uint16_t*>(ddst);
    }
    src = reinterpret_cast<const uint8_t*>(qsrc);
    count &= 3;
    // catch any remaining (will be < 4)
    while (--count >= 0) {
        *dst++ = ctable[*src++];
    }
}

#define SkSPRITE_ROW_PROC(d, s, n, x, y)    blitrow_d16_si8(d, s, n, ctable)

#define SkSPRITE_CLASSNAME                  Sprite_D16_SIndex8_Opaque
#define SkSPRITE_ARGS
#define SkSPRITE_FIELDS
#define SkSPRITE_INIT
#define SkSPRITE_DST_TYPE                   uint16_t
#define SkSPRITE_SRC_TYPE                   uint8_t
#define SkSPRITE_DST_GETADDR                getAddr16
#define SkSPRITE_SRC_GETADDR                getAddr8
#define SkSPRITE_PREAMBLE(srcBM, x, y)      const uint16_t* ctable = srcBM.getColorTable()->lock16BitCache()
#define SkSPRITE_BLIT_PIXEL(dst, src)       *dst = ctable[src]
#define SkSPRITE_NEXT_ROW
#define SkSPRITE_POSTAMBLE(srcBM)           srcBM.getColorTable()->unlock16BitCache()
#include "SkSpriteBlitterTemplate.h"

#define SkSPRITE_CLASSNAME                  Sprite_D16_SIndex8_Blend
#define SkSPRITE_ARGS                       , uint8_t alpha
#define SkSPRITE_FIELDS                     uint8_t fSrcAlpha;
#define SkSPRITE_INIT                       fSrcAlpha = alpha;
#define SkSPRITE_DST_TYPE                   uint16_t
#define SkSPRITE_SRC_TYPE                   uint8_t
#define SkSPRITE_DST_GETADDR                getAddr16
#define SkSPRITE_SRC_GETADDR                getAddr8
#define SkSPRITE_PREAMBLE(srcBM, x, y)      const uint16_t* ctable = srcBM.getColorTable()->lock16BitCache(); unsigned src_scale = SkAlpha255To256(fSrcAlpha);
#define SkSPRITE_BLIT_PIXEL(dst, src)       D16_S16_Blend_Pixel(dst, ctable[src], src_scale)
#define SkSPRITE_NEXT_ROW
#define SkSPRITE_POSTAMBLE(srcBM)           srcBM.getColorTable()->unlock16BitCache();
#include "SkSpriteBlitterTemplate.h"

///////////////////////////////////////////////////////////////////////////////

class Sprite_D16_S32_BlitRowProc : public SkSpriteBlitter {
public:
    Sprite_D16_S32_BlitRowProc(const SkBitmap& source)
        : SkSpriteBlitter(source) {}
    
    // overrides
    
    virtual void setup(const SkBitmap& device, int left, int top,
                       const SkPaint& paint) {
        this->INHERITED::setup(device, left, top, paint);
        
        unsigned flags = 0;
        
        if (paint.getAlpha() < 0xFF) {
            flags |= SkBlitRow::kGlobalAlpha_Flag;
        }
        if (!fSource->isOpaque()) {
            flags |= SkBlitRow::kSrcPixelAlpha_Flag;
        }
        if (paint.isDither()) {
            flags |= SkBlitRow::kDither_Flag;
        }
        fProc = SkBlitRow::Factory(flags, SkBitmap::kRGB_565_Config);
    }
    
    virtual void blitRect(int x, int y, int width, int height) {
#ifdef BLTSVILLE_ENHANCEMENT
        enum bverror bverr = BVERR_UNK;

        if(hbvlib) {
            struct bvbltparams params;
            struct bvbuffdesc srcdesc, dstdesc;
            struct bvsurfgeom srcgeom, dstgeom;

            int alpha = fPaint->getAlpha();
            int operation = 0;

            memset(&params, 0, sizeof(params));
            params.structsize = sizeof(params);

            params.src1.desc = &srcdesc;
            params.src1geom = &srcgeom;

            params.src2.desc = &dstdesc;
            params.src2geom = &dstgeom;

            params.dstdesc = &dstdesc;
            params.dstgeom = &dstgeom;

            memset(&srcgeom, 0, sizeof(srcgeom));
            srcgeom.structsize = sizeof(srcgeom);

            memset(&srcdesc, 0, sizeof(srcdesc));
            srcdesc.structsize = sizeof(srcdesc);

            memset(&dstgeom, 0, sizeof(dstgeom));
            dstgeom.structsize = sizeof(dstgeom);

            memset(&dstdesc, 0, sizeof(dstdesc));
            dstdesc.structsize = sizeof(dstdesc);
            srcgeom.format = OCDFMT_RGBx24;
            dstgeom.format = OCDFMT_RGB16;

            params.src2.desc = &dstdesc;
            params.src2geom = &dstgeom;

            params.src1rect.left = x - fLeft;
            params.src1rect.top = y - fTop;
            params.src1rect.width = width;
            params.src1rect.height = height;

            params.src2rect.left = x;
            params.src2rect.top = y;
            params.src2rect.width = width;
            params.src2rect.height = height;

            params.dstrect.left = x;
            params.dstrect.top = y;
            params.dstrect.width = width;
            params.dstrect.height = height;

            dstgeom.width = fDevice->width();
            dstgeom.height = fDevice->height();
            dstgeom.virtstride = fDevice->rowBytes();

            srcgeom.width = fSource->width();
            srcgeom.height = fSource->height();
            srcgeom.virtstride = fSource->rowBytes();

            srcdesc.virtaddr = fSource->getPixels();
            srcdesc.length = srcgeom.virtstride * srcgeom.height;

            dstdesc.virtaddr = fDevice->getPixels();
            dstdesc.length = dstgeom.virtstride * dstgeom.height;

            if(srcdesc.virtaddr == 0 || dstdesc.virtaddr == 0) {
                goto SKIA_D16_S32;
            }

            if(fPaint->isDither()) {
                params.dithermode = BVDITHER_FASTEST_ON;
            } else {
                params.dithermode = BVDITHER_NONE;
            }

            if (fPaint->getColorFilter() != NULL ||
                fPaint->getTypeface() != NULL ||
                fPaint->getShader() != NULL ||
                fPaint->getMaskFilter() != NULL ||
                fPaint->getLooper() != NULL) {
                goto SKIA_D16_S32;
            }

            SkXfermode* xfer = fPaint->getXfermode();

            SkXfermode::Mode mode;
            bool getMode = SkXfermode::IsMode(xfer, &mode);

            if (SkXfermode::kSrcOver_Mode == mode) {
                if (0 == alpha) {
                    mode = SkXfermode::kDst_Mode;
                } else if (0xFF == alpha && fSource->isOpaque()) {
                    mode = SkXfermode::kSrc_Mode;
                }
            }

            switch(mode) {

                case SkXfermode::kSrc_Mode:
                    if(alpha != 255) {
                        params.flags = BVFLAG_BLEND;
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1 + BVBLENDDEF_GLOBAL_UCHAR);
                        params.globalalpha.size8 = alpha;
                    } else {
                        params.flags = BVFLAG_ROP;
                        params.op.rop = 0xCCCC;
                    }

                    break;

                case SkXfermode::kSrcOver_Mode:
                    params.flags = BVFLAG_BLEND;

                    if(alpha == 255 && fSource->isOpaque()) {
                        operation = 1; //ROP 0xCCCC -dst RGB1|src RGBx
                    } else if(alpha == 255 && fDevice->isOpaque()) {
                        operation = 2; //BLEND -dst RGB1|src RGBA
                    } else if(alpha == 255) {
                        operation = 3; //BLEND -dst RGBA|src RGBA
                    } else if(fSource->isOpaque() && fDevice->isOpaque()) {
                        operation = 4; //BLEND -dst RGB1|src RGBx  + global alpha
                    } else if(fSource->isOpaque()) {
                        operation = 5; //BLEND -dst RGBA|src RGBx  + global alpha
                    } else if(fDevice->isOpaque()) {
                        operation = 6; //BLEND -dst RGB1|src RGBA  + global alpha
                    } else {
                        operation = 7; //BLEND -dst RGBA|src RGBA  + global alpha
                    }

                    switch(operation) {

                        case 1://ROP 0xCCCC -dst RGB1|src RGBx

                            if(alpha != 255) {
                                params.flags = BVFLAG_BLEND;
                                params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1 + BVBLENDDEF_GLOBAL_UCHAR);
                                params.globalalpha.size8 = alpha;
                            } else {
                                params.flags = BVFLAG_ROP;
                                params.op.rop = 0xCCCC;
                            }
                            srcgeom.format = OCDFMT_RGB124;
                            break;

                        case 2://BLEND -dst RGB1|src RGBA
                            params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER);
                            srcgeom.format = OCDFMT_RGBA24;
                            break;

                        case 3://BLEND -dst RGBA|src RGBA
                            params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER);
                            srcgeom.format = OCDFMT_RGBA24;
                            break;

                        case 4: //BLEND -dst RGB1|src RGBx  + global alpha
                            params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                            params.globalalpha.size8 = alpha;
                            srcgeom.format = OCDFMT_RGB124;
                            break;

                        case 5://BLEND -dst RGBA|src RGBx  + global alpha
                            params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                            params.globalalpha.size8 = alpha;
                            srcgeom.format = OCDFMT_RGB124;
                            break;

                        case 6://BLEND -dst RGB1|src RGBA  + global alpha
                            params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                            params.globalalpha.size8 = alpha;
                            srcgeom.format = OCDFMT_RGBA24;
                            break;

                        case 7://BLEND -dst RGBA|src RGBA  + global alpha
                            params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                            params.globalalpha.size8 = alpha;
                            srcgeom.format = OCDFMT_RGBA24;
                            break;
                        }

                    break;

                default:
                    goto SKIA_D16_S32;
                    break;
            }

            bverr = bv_blt(&params);
        }

SKIA_D16_S32:
        if(bverr != BVERR_NONE) {
#endif
        SK_RESTRICT uint16_t* dst = fDevice->getAddr16(x, y);
        const SK_RESTRICT SkPMColor* src = fSource->getAddr32(x - fLeft,
                                                              y - fTop);
        unsigned dstRB = fDevice->rowBytes();
        unsigned srcRB = fSource->rowBytes();
        SkBlitRow::Proc proc = fProc;
        U8CPU alpha = fPaint->getAlpha();
        
        while (--height >= 0) {
            proc(dst, src, width, alpha, x, y);
            y += 1;
            dst = (SK_RESTRICT uint16_t*)((char*)dst + dstRB);
            src = (const SK_RESTRICT SkPMColor*)((const char*)src + srcRB);
        }
#ifdef BLTSVILLE_ENHANCEMENT
        }
#endif
    }
    
private:
    SkBlitRow::Proc fProc;
    
    typedef SkSpriteBlitter INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

#include "SkTemplatesPriv.h"

SkSpriteBlitter* SkSpriteBlitter::ChooseD16(const SkBitmap& source,
                                            const SkPaint& paint,
                                            void* storage, size_t storageSize) {
    if (paint.getMaskFilter() != NULL) { // may add cases for this
        return NULL;
    }
    if (paint.getXfermode() != NULL) { // may add cases for this
        return NULL;
    }
    if (paint.getColorFilter() != NULL) { // may add cases for this
        return NULL;
    }

    SkSpriteBlitter* blitter = NULL;
    unsigned alpha = paint.getAlpha();

    switch (source.getConfig()) {
        case SkBitmap::kARGB_8888_Config:
            SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_S32_BlitRowProc,
                                  storage, storageSize, (source));
            break;
        case SkBitmap::kARGB_4444_Config:
            if (255 == alpha) {
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_S4444_Opaque,
                                      storage, storageSize, (source));
            } else {
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_S4444_Blend,
                                    storage, storageSize, (source, alpha >> 4));
            }
            break;
        case SkBitmap::kRGB_565_Config:
            if (255 == alpha) {
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_S16_Opaque,
                                      storage, storageSize, (source));
            } else {
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_S16_Blend,
                                      storage, storageSize, (source, alpha));
            }
            break;
        case SkBitmap::kIndex8_Config:
            if (paint.isDither()) {
                // we don't support dither yet in these special cases
                break;
            }
            if (source.isOpaque()) {
                if (255 == alpha) {
                    SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_SIndex8_Opaque,
                                          storage, storageSize, (source));
                } else {
                    SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_SIndex8_Blend,
                                         storage, storageSize, (source, alpha));
                }
            } else {
                if (255 == alpha) {
                    SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_SIndex8A_Opaque,
                                          storage, storageSize, (source));
                } else {
                    SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D16_SIndex8A_Blend,
                                         storage, storageSize, (source, alpha));
                }
            }
            break;
        default:
            break;
    }
    return blitter;
}

