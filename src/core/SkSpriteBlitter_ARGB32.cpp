/* libs/graphics/sgl/SkSpriteBlitter_ARGB32.cpp
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
#include "SkColorFilter.h"
#include "SkColorPriv.h"
#include "SkTemplates.h"
#include "SkUtils.h"
#include "SkXfermode.h"

#ifdef BLTSVILLE_ENHANCEMENT
#include <bltsville.h>
extern void* hbvlib;
extern BVFN_MAP bv_map;
extern BVFN_BLT bv_blt;
extern BVFN_UNMAP bv_unmap;
#endif

///////////////////////////////////////////////////////////////////////////////

class Sprite_D32_S32 : public SkSpriteBlitter {
public:
    Sprite_D32_S32(const SkBitmap& src, U8CPU alpha)  : INHERITED(src) {
        SkASSERT(src.config() == SkBitmap::kARGB_8888_Config);

        unsigned flags32 = 0;
        if (255 != alpha) {
            flags32 |= SkBlitRow::kGlobalAlpha_Flag32;
        }
        if (!src.isOpaque()) {
            flags32 |= SkBlitRow::kSrcPixelAlpha_Flag32;
        }

        fProc32 = SkBlitRow::Factory32(flags32);
        fAlpha = alpha;
    }

    virtual void blitRect(int x, int y, int width, int height) {
        SkASSERT(width > 0 && height > 0);
#ifdef BLTSVILLE_ENHANCEMENT
        enum bverror bverr = BVERR_UNK;

        if (hbvlib && (width > 1) && (height > 1)) {
            struct bvbltparams params;
            struct bvbuffdesc srcdesc, dstdesc;
            struct bvsurfgeom srcgeom, dstgeom;

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
                goto SKIA;
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

            switch(mode) {

                case SkXfermode::kSrc_Mode:
                    srcgeom.format = OCDFMT_RGBA24;
                    dstgeom.format = OCDFMT_RGBA24;

                    if(fAlpha != 255) {
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
                        srcgeom.format = OCDFMT_RGBA24;
                        dstgeom.format = OCDFMT_RGBA24;
                        break;

                    case 2://BLEND -dst RGB1|src RGBA
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER);
                        srcgeom.format = OCDFMT_RGBA24;
                        dstgeom.format = OCDFMT_RGB124;
                        break;

                    case 3://BLEND -dst RGBA|src RGBA
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER);
                        srcgeom.format = OCDFMT_RGBA24;
                        dstgeom.format = OCDFMT_RGBA24;
                        break;

                    case 4: //BLEND -dst RGB1|src RGBx  + global alpha
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                        params.globalalpha.size8 = alpha;
                        srcgeom.format = OCDFMT_RGB124;
                        dstgeom.format = OCDFMT_RGB124;
                        break;

                    case 5://BLEND -dst RGBA|src RGBx  + global alpha
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                        params.globalalpha.size8 = alpha;
                        srcgeom.format = OCDFMT_RGB124;
                        dstgeom.format = OCDFMT_RGBA24;
                        break;

                    case 6://BLEND -dst RGB1|src RGBA  + global alpha
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                        params.globalalpha.size8 = alpha;
                        srcgeom.format = OCDFMT_RGBA24;
                        dstgeom.format = OCDFMT_RGB124;
                        break;

                    case 7://BLEND -dst RGBA|src RGBA  + global alpha
                        params.op.blend = (enum bvblend)((unsigned int)BVBLEND_SRC1OVER + BVBLENDDEF_GLOBAL_UCHAR);
                        params.globalalpha.size8 = alpha;
                        srcgeom.format = OCDFMT_RGBA24;
                        dstgeom.format = OCDFMT_RGBA24;
                        break;
                    }

                    break;

                default:
                    goto SKIA;
                    break;
            }

            bverr = bv_blt(&params);
        }

SKIA:
        if(bverr != BVERR_NONE) {
#endif
        SK_RESTRICT uint32_t* dst = fDevice->getAddr32(x, y);
        const SK_RESTRICT uint32_t* src = fSource->getAddr32(x - fLeft,
                                                             y - fTop);
        size_t dstRB = fDevice->rowBytes();
        size_t srcRB = fSource->rowBytes();
        SkBlitRow::Proc32 proc = fProc32;
        U8CPU             alpha = fAlpha;

        do {
            proc(dst, src, width, alpha);
            dst = (SK_RESTRICT uint32_t*)((char*)dst + dstRB);
            src = (const SK_RESTRICT uint32_t*)((const char*)src + srcRB);
        } while (--height != 0);
#ifdef BLTSVILLE_ENHANCEMENT
        }
#endif
    }

private:
    SkBlitRow::Proc32   fProc32;
    U8CPU               fAlpha;

    typedef SkSpriteBlitter INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class Sprite_D32_XferFilter : public SkSpriteBlitter {
public:
    Sprite_D32_XferFilter(const SkBitmap& source, const SkPaint& paint)
        : SkSpriteBlitter(source) {
        fColorFilter = paint.getColorFilter();
        SkSafeRef(fColorFilter);

        fXfermode = paint.getXfermode();
        SkSafeRef(fXfermode);

        fBufferSize = 0;
        fBuffer = NULL;

        unsigned flags32 = 0;
        if (255 != paint.getAlpha()) {
            flags32 |= SkBlitRow::kGlobalAlpha_Flag32;
        }
        if (!source.isOpaque()) {
            flags32 |= SkBlitRow::kSrcPixelAlpha_Flag32;
        }

        fProc32 = SkBlitRow::Factory32(flags32);
        fAlpha = paint.getAlpha();
    }

    virtual ~Sprite_D32_XferFilter() {
        delete[] fBuffer;
        SkSafeUnref(fXfermode);
        SkSafeUnref(fColorFilter);
    }

    virtual void setup(const SkBitmap& device, int left, int top,
                       const SkPaint& paint) {
        this->INHERITED::setup(device, left, top, paint);

        int width = device.width();
        if (width > fBufferSize) {
            fBufferSize = width;
            delete[] fBuffer;
            fBuffer = new SkPMColor[width];
        }
    }

protected:
    SkColorFilter*      fColorFilter;
    SkXfermode*         fXfermode;
    int                 fBufferSize;
    SkPMColor*          fBuffer;
    SkBlitRow::Proc32   fProc32;
    U8CPU               fAlpha;

private:
    typedef SkSpriteBlitter INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

class Sprite_D32_S32A_XferFilter : public Sprite_D32_XferFilter {
public:
    Sprite_D32_S32A_XferFilter(const SkBitmap& source, const SkPaint& paint)
        : Sprite_D32_XferFilter(source, paint) {}

    virtual void blitRect(int x, int y, int width, int height) {
        SkASSERT(width > 0 && height > 0);
        SK_RESTRICT uint32_t* dst = fDevice->getAddr32(x, y);
        const SK_RESTRICT uint32_t* src = fSource->getAddr32(x - fLeft,
                                                             y - fTop);
        unsigned dstRB = fDevice->rowBytes();
        unsigned srcRB = fSource->rowBytes();
        SkColorFilter* colorFilter = fColorFilter;
        SkXfermode* xfermode = fXfermode;

        do {
            const SkPMColor* tmp = src;

            if (NULL != colorFilter) {
                colorFilter->filterSpan(src, width, fBuffer);
                tmp = fBuffer;
            }

            if (NULL != xfermode) {
                xfermode->xfer32(dst, tmp, width, NULL);
            } else {
                fProc32(dst, tmp, width, fAlpha);
            }

            dst = (SK_RESTRICT uint32_t*)((char*)dst + dstRB);
            src = (const SK_RESTRICT uint32_t*)((const char*)src + srcRB);
        } while (--height != 0);
    }

private:
    typedef Sprite_D32_XferFilter INHERITED;
};

static void fillbuffer(SK_RESTRICT SkPMColor dst[],
                       const SK_RESTRICT SkPMColor16 src[], int count) {
    SkASSERT(count > 0);

    do {
        *dst++ = SkPixel4444ToPixel32(*src++);
    } while (--count != 0);
}

class Sprite_D32_S4444_XferFilter : public Sprite_D32_XferFilter {
public:
    Sprite_D32_S4444_XferFilter(const SkBitmap& source, const SkPaint& paint)
        : Sprite_D32_XferFilter(source, paint) {}

    virtual void blitRect(int x, int y, int width, int height) {
        SkASSERT(width > 0 && height > 0);
        SK_RESTRICT SkPMColor* dst = fDevice->getAddr32(x, y);
        const SK_RESTRICT SkPMColor16* src = fSource->getAddr16(x - fLeft,
                                                                y - fTop);
        unsigned dstRB = fDevice->rowBytes();
        unsigned srcRB = fSource->rowBytes();
        SK_RESTRICT SkPMColor* buffer = fBuffer;
        SkColorFilter* colorFilter = fColorFilter;
        SkXfermode* xfermode = fXfermode;

        do {
            fillbuffer(buffer, src, width);

            if (NULL != colorFilter) {
                colorFilter->filterSpan(buffer, width, buffer);
            }
            if (NULL != xfermode) {
                xfermode->xfer32(dst, buffer, width, NULL);
            } else {
                fProc32(dst, buffer, width, fAlpha);
            }

            dst = (SK_RESTRICT SkPMColor*)((char*)dst + dstRB);
            src = (const SK_RESTRICT SkPMColor16*)((const char*)src + srcRB);
        } while (--height != 0);
    }

private:
    typedef Sprite_D32_XferFilter INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

static void src_row(SK_RESTRICT SkPMColor dst[],
                    const SK_RESTRICT SkPMColor16 src[], int count) {
    do {
        *dst = SkPixel4444ToPixel32(*src);
        src += 1;
        dst += 1;
    } while (--count != 0);
}

class Sprite_D32_S4444_Opaque : public SkSpriteBlitter {
public:
    Sprite_D32_S4444_Opaque(const SkBitmap& source) : SkSpriteBlitter(source) {}

    virtual void blitRect(int x, int y, int width, int height) {
        SkASSERT(width > 0 && height > 0);
        SK_RESTRICT SkPMColor* dst = fDevice->getAddr32(x, y);
        const SK_RESTRICT SkPMColor16* src = fSource->getAddr16(x - fLeft,
                                                                y - fTop);
        unsigned dstRB = fDevice->rowBytes();
        unsigned srcRB = fSource->rowBytes();

        do {
            src_row(dst, src, width);
            dst = (SK_RESTRICT SkPMColor*)((char*)dst + dstRB);
            src = (const SK_RESTRICT SkPMColor16*)((const char*)src + srcRB);
        } while (--height != 0);
    }
};

static void srcover_row(SK_RESTRICT SkPMColor dst[],
                        const SK_RESTRICT SkPMColor16 src[], int count) {
    do {
        *dst = SkPMSrcOver(SkPixel4444ToPixel32(*src), *dst);
        src += 1;
        dst += 1;
    } while (--count != 0);
}

class Sprite_D32_S4444 : public SkSpriteBlitter {
public:
    Sprite_D32_S4444(const SkBitmap& source) : SkSpriteBlitter(source) {}

    virtual void blitRect(int x, int y, int width, int height) {
        SkASSERT(width > 0 && height > 0);
        SK_RESTRICT SkPMColor* dst = fDevice->getAddr32(x, y);
        const SK_RESTRICT SkPMColor16* src = fSource->getAddr16(x - fLeft,
                                                                y - fTop);
        unsigned dstRB = fDevice->rowBytes();
        unsigned srcRB = fSource->rowBytes();

        do {
            srcover_row(dst, src, width);
            dst = (SK_RESTRICT SkPMColor*)((char*)dst + dstRB);
            src = (const SK_RESTRICT SkPMColor16*)((const char*)src + srcRB);
        } while (--height != 0);
    }
};

#ifdef BLTSVILLE_ENHANCEMENT
class Sprite_D32_S16_Opaque : public SkSpriteBlitter {
public:
    Sprite_D32_S16_Opaque(const SkBitmap& source)
        : SkSpriteBlitter(source) {}

    // overrides
    virtual void blitRect(int x, int y, int width, int height) {
        enum bverror bverr = BVERR_UNK;

        if (hbvlib && (width > 1) && (height > 1)) {
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
            dstgeom.format = OCDFMT_RGB124;

            memset(&dstdesc, 0, sizeof(dstdesc));
            dstdesc.structsize = sizeof(dstdesc);

            params.flags = BVFLAG_ROP;
            params.op.rop = 0xCCCC;

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
                goto SKIA_D32_S16;
            }

            bverr = bv_blt(&params);
        }
SKIA_D32_S16:
        if(bverr != BVERR_NONE) {
            SK_RESTRICT uint32_t* dst = fDevice->getAddr32(x, y);
            const SK_RESTRICT uint16_t* src = fSource->getAddr16(x - fLeft, y - fTop);
            unsigned dstRB = fDevice->rowBytes();
            unsigned srcRB = fSource->rowBytes();
            int i;

            while (--height >= 0) {
                for ( i = 0; i < width; i++ ) {
                    dst[i] = SkPixel16ToPixel32(src[i]);
                }
                dst = (uint32_t*)((char*)dst + dstRB);
                src = (const uint16_t*)((const char*)src + srcRB);
            }
        }
    }
};
#endif

///////////////////////////////////////////////////////////////////////////////

#include "SkTemplatesPriv.h"

SkSpriteBlitter* SkSpriteBlitter::ChooseD32(const SkBitmap& source,
                                            const SkPaint& paint,
                                            void* storage, size_t storageSize) {
    if (paint.getMaskFilter() != NULL) {
        return NULL;
    }

    U8CPU       alpha = paint.getAlpha();
    SkXfermode* xfermode = paint.getXfermode();
    SkColorFilter* filter = paint.getColorFilter();
    SkSpriteBlitter* blitter = NULL;

    switch (source.getConfig()) {
        case SkBitmap::kARGB_4444_Config:
            if (alpha != 0xFF) {
                return NULL;    // we only have opaque sprites
            }
            if (xfermode || filter) {
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D32_S4444_XferFilter,
                                      storage, storageSize, (source, paint));
            } else if (source.isOpaque()) {
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D32_S4444_Opaque,
                                      storage, storageSize, (source));
            } else {
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D32_S4444,
                                      storage, storageSize, (source));
            }
            break;
        case SkBitmap::kARGB_8888_Config:
            if (xfermode || filter) {
                if (255 == alpha) {
                    // this can handle xfermode or filter, but not alpha
                    SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D32_S32A_XferFilter,
                                      storage, storageSize, (source, paint));
                }
            } else {
                // this can handle alpha, but not xfermode or filter
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D32_S32,
                              storage, storageSize, (source, alpha));
            }
            break;
#ifdef BLTSVILLE_ENHANCEMENT
        case SkBitmap::kRGB_565_Config:
            if (hbvlib && (255 == alpha)) {
                SK_PLACEMENT_NEW_ARGS(blitter, Sprite_D32_S16_Opaque,
                                      storage, storageSize, (source));
            }
            break;
#endif
        default:
            break;
    }
    return blitter;
}

