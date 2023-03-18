//
// PNG Decoder
//
// written by Larry Bank
// bitbank@pobox.com
// Arduino port started 5/3/2021
// Original PNG code written 20+ years ago :)
// The goal of this code is to decode PNG images on embedded systems
//
// Copyright 2021 BitBank Software, Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================
//
#include "zlib.h"

//
// Verify it's a PNG file and then parse the IHDR chunk
// to get basic image size/type/etc
//
PNG_STATIC int PNGParseInfo(PNGIMAGE *pPage)
{
    uint8_t *s = pPage->ucFileBuf;
    int iBytesRead;
    
    pPage->iHasAlpha = pPage->iInterlaced = 0;
    // Read a few bytes to just parse the size/pixel info
    iBytesRead = (*pPage->pfnRead)(&pPage->PNGFile, s, 32);
    if (iBytesRead < 32) { // a PNG file this tiny? probably bad
        pPage->iError = PNG_INVALID_FILE;
        return pPage->iError;
    }

    if (MOTOLONG(s) != (int32_t)0x89504e47) { // check that it's a PNG file
        pPage->iError = PNG_INVALID_FILE;
        return pPage->iError;
    }
    if (MOTOLONG(&s[12]) == 0x49484452/*'IHDR'*/) {
        pPage->iWidth = MOTOLONG(&s[16]);
        pPage->iHeight = MOTOLONG(&s[20]);
        pPage->ucBpp = s[24]; // bits per pixel
        pPage->ucPixelType = s[25]; // pixel type
        pPage->iInterlaced = s[28];
        if (pPage->iInterlaced || pPage->ucBpp > 8) { // 16-bit pixels are not supported (yet)
            pPage->iError = PNG_UNSUPPORTED_FEATURE;
            return pPage->iError;
        }
        // calculate the number of bytes per line of pixels
        switch (pPage->ucPixelType) {
            case PNG_PIXEL_GRAYSCALE: // grayscale
            case PNG_PIXEL_INDEXED: // indexed
                pPage->iPitch = (pPage->iWidth * pPage->ucBpp + 7)/8; // bytes per pixel
                break;
            case PNG_PIXEL_TRUECOLOR: // truecolor
                pPage->iPitch = ((3 * pPage->ucBpp) * pPage->iWidth + 7)/8;
                break;
            case PNG_PIXEL_GRAY_ALPHA: // grayscale + alpha
                pPage->iPitch = ((2 * pPage->ucBpp) * pPage->iWidth + 7)/8;
                pPage->iHasAlpha = 1;
                break;
            case PNG_PIXEL_TRUECOLOR_ALPHA: // truecolor + alpha
                pPage->iPitch = ((4 * pPage->ucBpp) * pPage->iWidth + 7)/8;
                pPage->iHasAlpha = 1;
        } // switch
    }
    if (pPage->iPitch >= PNG_MAX_BUFFERED_PIXELS)
       return PNG_TOO_BIG;

    return PNG_SUCCESS;
} /* PNGParseInfo() */
//
// De-filter the current line of pixels
//
PNG_STATIC void DeFilter(uint8_t *pCurr, uint8_t *pPrev, int iWidth, int iPitch)
{
    uint8_t ucFilter = *pCurr++;
    int x, iBpp;
    if (iPitch <= iWidth)
        iBpp = 1;
    else
        iBpp = iPitch / iWidth;
    
    pPrev++; // skip filter of previous line
    switch (ucFilter) { // switch on filter type
        case PNG_FILTER_NONE:
            // nothing to do :)
            break;
        case PNG_FILTER_SUB:
            for (x=iBpp; x<iPitch; x++) {
                pCurr[x] += pCurr[x-iBpp];
            }
            break;
        case PNG_FILTER_UP:
            for (x = 0; x < iPitch; x++) {
               pCurr[x] += pPrev[x];
            }
            break;
        case PNG_FILTER_AVG:
            for (x = 0; x < iBpp; x++) {
               pCurr[x] = (pCurr[x] +
                  pPrev[x] / 2 );
            }
            for (x = iBpp; x < iPitch; x++) {
               pCurr[x] = pCurr[x] +
                  (pPrev[x] + pCurr[x-iBpp]) / 2;
            }
            break;
        case PNG_FILTER_PAETH:
            if (iBpp == 1) {
                int a, c;
                uint8_t *pEnd = &pCurr[iPitch];
                // First pixel/byte
                c = *pPrev++;
                a = *pCurr + c;
                *pCurr++ = (uint8_t)a;
                while (pCurr < pEnd) {
                   int b, pa, pb, pc, p;
                   a &= 0xff; // From previous iteration
                   b = *pPrev++;
                   p = b - c;
                   pc = a - c;
                   // assume no native ABS() instruction
                   pa = p < 0 ? -p : p;
                   pb = pc < 0 ? -pc : pc;
                   pc = (p + pc) < 0 ? -(p + pc) : p + pc;
                   // choose the best predictor
                   if (pb < pa) {
                      pa = pb; a = b;
                   }
                   if (pc < pa) a = c;
                   // Calculate current pixel
                   c = b;
                   a += *pCurr;
                   *pCurr++ = (uint8_t)a;
                }
            } else { // multi-byte
                uint8_t *pEnd = &pCurr[iBpp];
                // first pixel is treated the same as 'up'
                while (pCurr < pEnd) {
                   int a = *pCurr + *pPrev++;
                   *pCurr++ = (uint8_t)a;
                }
                pEnd = pEnd + (iPitch - iBpp);
                while (pCurr < pEnd) {
                   int a, b, c, pa, pb, pc, p;
                   c = pPrev[-iBpp];
                   a = pCurr[-iBpp];
                   b = *pPrev++;
                   p = b - c;
                   pc = a - c;
                    // asume no native ABS() instruction
                   pa = p < 0 ? -p : p;
                   pb = pc < 0 ? -pc : pc;
                   pc = (p + pc) < 0 ? -(p + pc) : p + pc;
                   if (pb < pa) {
                      pa = pb; a = b;
                   }
                   if (pc < pa) a = c;
                   a += *pCurr;
                   *pCurr++ = (uint8_t)a;
                }
            } // multi-byte
            break;
    } // switch on filter type
} /* DeFilter() */
//
// PNGInit
// Parse the PNG file header and confirm that it's a valid file
//
// returns 0 for success, 1 for failure
//
PNG_STATIC int PNGInit(PNGIMAGE *pPNG)
{
    return PNGParseInfo(pPNG); // gather info for image
} /* PNGInit() */
//
// Decode the PNG file
//
// You must call open() before calling decode()
// This function can be called repeatedly without having
// to close and re-open the file
//
PNG_STATIC int DecodePNG(PNGIMAGE *pPage, void *pUser, int iOptions)
{
    int err, y, iLen=0;
    int bDone, iOffset, iFileOffset, iBytesRead;
    int iMarker=0;
    uint8_t *tmp, *pCurr, *pPrev;
    z_stream d_stream; /* decompression stream */
    uint8_t *s = pPage->ucFileBuf;
    struct inflate_state *state;
    
    // Either the image buffer must be allocated or a draw callback must be set before entering
    if (pPage->pImage == NULL && pPage->pfnDraw == NULL) {
        pPage->iError = PNG_NO_BUFFER;
        return 0;
    }
    // Use internal buffer to maintain the current and previous lines
    pCurr = pPage->ucPixels;
    pPrev = &pPage->ucPixels[pPage->iPitch+1];
    pPage->iError = PNG_SUCCESS;
    // Start decoding the image
    bDone = FALSE;
    // Inflate the compressed image data
    // The allocation functions are disabled and zlib has been modified
    // to not use malloc/free and instead the buffer is part of the PNG class
    d_stream.zalloc = (alloc_func)0;
    d_stream.zfree = (free_func)0;
    d_stream.opaque = (voidpf)0;
    // Insert the memory pointer here to avoid having to use malloc() inside zlib
    state = (struct inflate_state FAR *)pPage->ucZLIB;
    d_stream.state = (struct internal_state FAR *)state;
    state->window = &pPage->ucZLIB[sizeof(inflate_state)]; // point to 32k dictionary buffer
    err = inflateInit(&d_stream);
#ifdef FUTURE
//    if (inpage->cCompression == PIL_COMP_IPHONE_FLATE)
//        err = mz_inflateInit2(&d_stream, -15); // undocumented option which ignores header and crcs
//    else
//        err = mz_inflateInit2(&d_stream, 15);
#endif // FUTURE
    
    iFileOffset = 8; // skip PNG file signature
    iOffset = 0; // internal buffer offset starts at 0
    // Read some data to start
    (*pPage->pfnSeek)(&pPage->PNGFile, iFileOffset);
    iBytesRead = (*pPage->pfnRead)(&pPage->PNGFile, s, PNG_FILE_BUF_SIZE);
    iFileOffset += iBytesRead;
    y = 0;
    d_stream.avail_out = 0;
    d_stream.next_out = pPage->pImage;

    while (y < pPage->iHeight) { // continue until fully decoded
        // parse the markers until the next data block
    while (!bDone)
    {
        iLen = MOTOLONG(&s[iOffset]); // chunk length
        if (iLen < 0 || iLen + (iFileOffset - iBytesRead) > pPage->PNGFile.iSize) // invalid data
        {
            pPage->iError = PNG_DECODE_ERROR;
            return 1;
        }
        iMarker = MOTOLONG(&s[iOffset+4]);
        iOffset += 8; // point to the marker data
        switch (iMarker)
        {
            case 0x44474b62: // 'bKGD' DEBUG
                break;
            case 0x67414d41: //'gAMA'
                break;
#ifdef FUTURE
            case 0x6663544C: //'fcTL' frame control block for animated PNG (need to get size of this partial image)
                pPage->iWidth = MOTOLONG(&pPage->pData[iOffset + 4]); // frame width
                pPage->iHeight = MOTOLONG(&pPage->pData[iOffset + 8]); // frame height
                bDone = TRUE;
                break;
#endif
            case 0x504c5445: {//'PLTE' palette colors
                int count = iLen / 3;
                const uint8_t* p = s + iOffset;
                for (int i = 0; i < count; ++i) {
                    pPage->ucPalette[i] = roo_display::Color(p[0], p[1], p[2]);
                    p += 3;
                }
                break;
            }
            case 0x74524e53: { //'tRNS' transparency info
                if (pPage->ucPixelType == PNG_PIXEL_INDEXED) { // if palette exists
                    const uint8_t* p = s + iOffset;
                    for (int i = 0; i < iLen; ++i) {
                        pPage->ucPalette[i].set_a(*p++);
                    }
                    pPage->iHasAlpha = 1;
                }
                else if (iLen == 2) // for grayscale images
                {
                    pPage->iTransparent = s[iOffset + 1]; // lower part of 2-byte value is transparent color index
                    pPage->iHasAlpha = 1;
                }
                else if (iLen == 6) // transparent color for 24-bpp image
                {
                    pPage->iTransparent = s[iOffset + 5]; // lower part of 2-byte value is transparent color value
                    pPage->iTransparent |= (s[iOffset + 3] << 8);
                    pPage->iTransparent |= (s[iOffset + 1] << 16);
                    pPage->iHasAlpha = 1;
                }
                break;
            }
            case 0x49444154: //'IDAT' image data block
                while (iLen) {
                    if (iOffset >= iBytesRead) {
                        // we ran out of data; get some more
                        iBytesRead = (*pPage->pfnRead)(&pPage->PNGFile, pPage->ucFileBuf, (iLen > PNG_FILE_BUF_SIZE) ? PNG_FILE_BUF_SIZE : iLen);
                        iFileOffset += iBytesRead;
                        iOffset = 0;
                    } else {
                        // number of bytes remaining in buffer
                        iBytesRead -= iOffset;
                    }
                    d_stream.next_in  = &pPage->ucFileBuf[iOffset];
                    d_stream.avail_in = iBytesRead;
                    iLen -= iBytesRead;
                    if (iLen < 0) iLen = 0;
                    iOffset += iBytesRead;
            //        if (iMarker == 0x66644154) // data starts at offset 4 in APNG frame data block
            //        {
            //            d_stream.next_in += 4;
            //            d_stream.avail_in -= 4;
            //        }
                    err = 0;
                    while (err == Z_OK) {
                        if (d_stream.avail_out == 0) { // reset for next line
                            d_stream.avail_out = pPage->iPitch+1;
                            d_stream.next_out = pCurr;
                        } // otherwise it could be a continuation of an unfinished line
                        err = inflate(&d_stream, Z_NO_FLUSH, iOptions & PNG_CHECK_CRC);
                        if ((err == Z_OK || err == Z_STREAM_END) && d_stream.avail_out == 0) {// successfully decoded line
                            DeFilter(pCurr, pPrev, pPage->iWidth, pPage->iPitch);
                            if (pPage->pImage == NULL) { // no image buffer, send it line by line
                                PNGDRAW pngd;
                                pngd.pUser = pUser;
                                pngd.iPitch = pPage->iPitch;
                                pngd.iWidth = pPage->iWidth;
                                pngd.pPalette = pPage->ucPalette;
                                pngd.pFastPalette = (iOptions & PNG_FAST_PALETTE) ? (uint16_t *)&pPage->ucPixels[sizeof(pPage->ucPixels)-512] : NULL;
                                pngd.pPixels = pCurr+1;
                                pngd.iPixelType = pPage->ucPixelType;
                                pngd.iHasAlpha = pPage->iHasAlpha;
                                pngd.iBpp = pPage->ucBpp;
                                pngd.y = y;
                                (*pPage->pfnDraw)(&pngd);
                            } else {
                                // copy to destination bitmap
                                memcpy(&pPage->pImage[y * pPage->iPitch], &pCurr[1], pPage->iPitch);
                            }
                            y++;
                        // swap current and previous lines
                        tmp = pCurr; pCurr = pPrev; pPrev = tmp;
                        } else { // some error
                            tmp = NULL;
                        }
                    }
                    if (err == Z_STREAM_END && d_stream.avail_out == 0) {
                        // successful decode, stop here
                        y = pPage->iHeight;
                        bDone = TRUE;
                    } else  if (err == Z_DATA_ERROR || err == Z_STREAM_ERROR) {
                        iLen = 0; // quit now
                        y = pPage->iHeight;
                        pPage->iError = PNG_DECODE_ERROR;
                        bDone = TRUE; // force loop to exit with error
                    } else if (err == Z_BUF_ERROR) {
                        y |= 0; // need more data
                    }
                } // while (iLen)
                if (y != pPage->iHeight && iFileOffset < pPage->PNGFile.iSize) {
                    // need to read more IDAT chunks
                    iBytesRead = (*pPage->pfnRead)(&pPage->PNGFile, pPage->ucFileBuf,  PNG_FILE_BUF_SIZE);
                    iFileOffset += iBytesRead;
                    iOffset = 0;
                }
                break;
                //               case 0x69545874: //'iTXt'
                //               case 0x7a545874: //'zTXt'
#ifdef FUTURE
            case 0x74455874: //'tEXt'
            {
                char szTemp[256];
                char *pDest = NULL;
                memcpy(szTemp, &s[iOffset], 80); // get the label length (Title, Author, Description, Copyright, Creation Time, Software, Disclaimer, Warning, Source, Comment)
                i = (int)strlen(szTemp) + 1; // start of actual text
                if (strcmp(szTemp, "Comment") == 0 || strcmp(szTemp, "Description") == 0) pDest = &pPage->szComment[0];
                else if (strcmp(szTemp, "Software") == 0) pDest = &pPage->szSoftware[0];
                else if (strcmp(szTemp, "Author") == 0) pDest = &pPage->szArtist[0];
                if (pDest != NULL)
                {
                    if ((iLen - i) < 128)
                    {
                        memcpy(pPage->szComment, &pPage->pData[iOffset + i], iLen - i);
                        pPage->szComment[iLen - i + 1] = 0;
                    }
                    else
                    {
                        memcpy(pPage->szComment, &pPage->pData[iOffset + i], 127);
                        pPage->szComment[127] = '\0';
                    }
                }
            }
                break;
#endif
        } // switch
        iOffset += (iLen + 4); // skip data + CRC
        if (iOffset > iBytesRead-8) { // need to read more data
            iFileOffset += (iOffset - iBytesRead);
            (*pPage->pfnSeek)(&pPage->PNGFile, iFileOffset);
            iBytesRead = (*pPage->pfnRead)(&pPage->PNGFile, s, PNG_FILE_BUF_SIZE);
            iFileOffset += iBytesRead;
            iOffset = 0;
        }
    } // while !bDone
    } // while y < height
    err = inflateEnd(&d_stream);
    return pPage->iError;
} /* DecodePNG() */
