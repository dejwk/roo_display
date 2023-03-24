#include "roo_display/image/png/png.h"

#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/image/png/lib/png.inl"

PNG_STATIC int PNGInit(PNGIMAGE *pPNG);
PNG_STATIC int DecodePNG(PNGIMAGE *pImage, void *pUser, int iOptions);
// Include the C code which does the actual work
// #include "png.inl"

namespace roo_display {

int32_t png_read(PNGFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  PngDecoder *decoder = (PngDecoder *)pFile->fHandle;
  return decoder->input_->read(pBuf, iLen);
}

int32_t png_seek(PNGFILE *pFile, int32_t iPosition) {
  PngDecoder *decoder = (PngDecoder *)pFile->fHandle;
  return decoder->input_->seek(iPosition);
}

namespace {

struct User {
  const Surface* surface;
  const Palette* palette;
};

}

void png_draw(PNGDRAW *pDraw) {
  User& user = *((User*)pDraw->pUser);
  const Surface *surface = user.surface;
  int16_t y = pDraw->y;
  if (y < surface->clip_box().yMin() || y > surface->clip_box().yMax()) return;
  Box box(0, y, pDraw->iWidth - 1, y);
  switch (pDraw->iPixelType) {
    case PNG_PIXEL_TRUECOLOR_ALPHA: {
      ConstDramRaster<Rgba8888> raster(box, pDraw->pPixels);
      surface->drawObject(raster);
      break;
    }
    case PNG_PIXEL_TRUECOLOR: {
      ConstDramRaster<Rgb888> raster(box, pDraw->pPixels);
      surface->drawObject(raster);
      break;
    }
    case PNG_PIXEL_GRAYSCALE: {
      ConstDramRaster<Grayscale8> raster(box, pDraw->pPixels);
      surface->drawObject(raster);
      break;
    }
    case PNG_PIXEL_GRAY_ALPHA: {
      ConstDramRaster<GrayAlpha8> raster(box, pDraw->pPixels);
      surface->drawObject(raster);
      break;
    }
    case PNG_PIXEL_INDEXED: {
      switch (pDraw->iBpp) {
        case 8: {
          ConstDramRaster<Indexed8> raster(box, pDraw->pPixels, Indexed8(user.palette));
          surface->drawObject(raster);
          break;
        }
        case 4: {
          ConstDramRaster<Indexed4> raster(box, pDraw->pPixels, Indexed4(user.palette));
          surface->drawObject(raster);
          break;
        }
        case 2: {
          ConstDramRaster<Indexed2> raster(box, pDraw->pPixels, Indexed2(user.palette));
          surface->drawObject(raster);
          break;
        }
        case 1: {
          ConstDramRaster<Indexed1> raster(box, pDraw->pPixels, Indexed1(user.palette));
          surface->drawObject(raster);
          break;
        }
      }
    }
  }
}

PngDecoder::PngDecoder() : pngdec_(new PNGIMAGE()), input_(nullptr) {}

bool PngDecoder::openInternal(int16_t &width, int16_t &height) {
  memset(pngdec_.get(), 0, sizeof(PNGIMAGE));
  pngdec_->pfnRead = png_read;
  pngdec_->pfnSeek = png_seek;
  pngdec_->pfnDraw = png_draw;
  pngdec_->pfnOpen = nullptr;
  pngdec_->pfnClose = nullptr;
  pngdec_->PNGFile.iSize = input_->size();

  pngdec_->PNGFile.fHandle = this;
  if (PNGInit(pngdec_.get()) != PNG_SUCCESS) return false;
  width = pngdec_->iWidth;
  height = pngdec_->iHeight;
  return true;
}

void PngDecoder::drawInternal(const Surface &s, uint8_t scale) {
  Box extents(0, 0, pngdec_->iWidth - 1, pngdec_->iHeight - 1);
  if (Box::Intersect(s.clip_box(), extents.translate(s.dx(), s.dy())).empty()) return;
  if (pngdec_->ucPixelType == PNG_PIXEL_INDEXED) {
    palette_ = Palette::ReadOnly((Color*)pngdec_->ucPalette, 1 << pngdec_->ucBpp);
  }
  User user{.surface = &s, .palette = &palette_};
  DecodePNG(pngdec_.get(), (void *)&user, 0);
}

}  // namespace roo_display
