/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2003                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id: dct_encode.c,v 1.8 2003/06/10 01:31:33 tterribe Exp $

 ********************************************************************/

#include <stdlib.h>
#include "encoder_internal.h"

static int ModeUsesMC[MAX_MODES] = { 0, 0, 1, 1, 1, 0, 1, 1 };

static void Sub8 (unsigned char *FiltPtr, unsigned char *ReconPtr,
                  ogg_int16_t *DctInputPtr, unsigned char *old_ptr1,
                  unsigned char *new_ptr1, ogg_uint32_t PixelsPerLine,
                  ogg_uint32_t ReconPixelsPerLine ) {
  int i;

  /* For each block row */
  for ( i=0; i<BLOCK_HEIGHT_WIDTH; i++ ){
    DctInputPtr[0] = (ogg_int16_t)((int)(FiltPtr[0]) - ((int)ReconPtr[0]) );
    DctInputPtr[1] = (ogg_int16_t)((int)(FiltPtr[1]) - ((int)ReconPtr[1]) );
    DctInputPtr[2] = (ogg_int16_t)((int)(FiltPtr[2]) - ((int)ReconPtr[2]) );
    DctInputPtr[3] = (ogg_int16_t)((int)(FiltPtr[3]) - ((int)ReconPtr[3]) );
    DctInputPtr[4] = (ogg_int16_t)((int)(FiltPtr[4]) - ((int)ReconPtr[4]) );
    DctInputPtr[5] = (ogg_int16_t)((int)(FiltPtr[5]) - ((int)ReconPtr[5]) );
    DctInputPtr[6] = (ogg_int16_t)((int)(FiltPtr[6]) - ((int)ReconPtr[6]) );
    DctInputPtr[7] = (ogg_int16_t)((int)(FiltPtr[7]) - ((int)ReconPtr[7]) );

    /* Update the screen canvas in one step*/
    ((ogg_uint32_t*)old_ptr1)[0] = ((ogg_uint32_t*)new_ptr1)[0];
    ((ogg_uint32_t*)old_ptr1)[1] = ((ogg_uint32_t*)new_ptr1)[1];

    /* Start next row */
    new_ptr1 += PixelsPerLine;
    old_ptr1 += PixelsPerLine;
    FiltPtr += PixelsPerLine;
    ReconPtr += ReconPixelsPerLine;
    DctInputPtr += BLOCK_HEIGHT_WIDTH;
  }
}

static void Sub8_128 (unsigned char *FiltPtr, ogg_int16_t *DctInputPtr,
                      unsigned char *old_ptr1, unsigned char *new_ptr1,
                      ogg_uint32_t PixelsPerLine ) {
  int i;
  /* For each block row */
  for ( i=0; i<BLOCK_HEIGHT_WIDTH; i++ ){
    /* INTRA mode so code raw image data */
    /* We convert the data to 8 bit signed (by subtracting 128) as
       this reduces the internal precision requirments in the DCT
       transform. */
    DctInputPtr[0] = (ogg_int16_t)((int)(FiltPtr[0]) - 128);
    DctInputPtr[1] = (ogg_int16_t)((int)(FiltPtr[1]) - 128);
    DctInputPtr[2] = (ogg_int16_t)((int)(FiltPtr[2]) - 128);
    DctInputPtr[3] = (ogg_int16_t)((int)(FiltPtr[3]) - 128);
    DctInputPtr[4] = (ogg_int16_t)((int)(FiltPtr[4]) - 128);
    DctInputPtr[5] = (ogg_int16_t)((int)(FiltPtr[5]) - 128);
    DctInputPtr[6] = (ogg_int16_t)((int)(FiltPtr[6]) - 128);
    DctInputPtr[7] = (ogg_int16_t)((int)(FiltPtr[7]) - 128);

    /* Update the screen canvas in one step */
    ((ogg_uint32_t*)old_ptr1)[0] = ((ogg_uint32_t*)new_ptr1)[0];
    ((ogg_uint32_t*)old_ptr1)[1] = ((ogg_uint32_t*)new_ptr1)[1];

    /* Start next row */
    new_ptr1 += PixelsPerLine;
    old_ptr1 += PixelsPerLine;
    FiltPtr += PixelsPerLine;
    DctInputPtr += BLOCK_HEIGHT_WIDTH;
  }
}

static void Sub8Av2 (unsigned char *FiltPtr, unsigned char *ReconPtr1,
                     unsigned char *ReconPtr2, ogg_int16_t *DctInputPtr,
                     unsigned char *old_ptr1, unsigned char *new_ptr1,
                     ogg_uint32_t PixelsPerLine,
                     ogg_uint32_t ReconPixelsPerLine ) {
  int i;

  /* For each block row */
  for ( i=0; i<BLOCK_HEIGHT_WIDTH; i++ ) {
    DctInputPtr[0] = (ogg_int16_t)
      ((int)(FiltPtr[0]) - (((int)ReconPtr1[0] + (int)ReconPtr2[0]) / 2) );
    DctInputPtr[1] = (ogg_int16_t)
      ((int)(FiltPtr[1]) - (((int)ReconPtr1[1] + (int)ReconPtr2[1]) / 2) );
    DctInputPtr[2] = (ogg_int16_t)
      ((int)(FiltPtr[2]) - (((int)ReconPtr1[2] + (int)ReconPtr2[2]) / 2) );
    DctInputPtr[3] = (ogg_int16_t)
      ((int)(FiltPtr[3]) - (((int)ReconPtr1[3] + (int)ReconPtr2[3]) / 2) );
    DctInputPtr[4] = (ogg_int16_t)
      ((int)(FiltPtr[4]) - (((int)ReconPtr1[4] + (int)ReconPtr2[4]) / 2) );
    DctInputPtr[5] = (ogg_int16_t)
      ((int)(FiltPtr[5]) - (((int)ReconPtr1[5] + (int)ReconPtr2[5]) / 2) );
    DctInputPtr[6] = (ogg_int16_t)
      ((int)(FiltPtr[6]) - (((int)ReconPtr1[6] + (int)ReconPtr2[6]) / 2) );
    DctInputPtr[7] = (ogg_int16_t)
      ((int)(FiltPtr[7]) - (((int)ReconPtr1[7] + (int)ReconPtr2[7]) / 2) );

    /* Update the screen canvas in one step */
    ((ogg_uint32_t*)old_ptr1)[0] = ((ogg_uint32_t*)new_ptr1)[0];
    ((ogg_uint32_t*)old_ptr1)[1] = ((ogg_uint32_t*)new_ptr1)[1];

    /* Start next row */
    new_ptr1 += PixelsPerLine;
    old_ptr1 += PixelsPerLine;
    FiltPtr += PixelsPerLine;
    ReconPtr1 += ReconPixelsPerLine;
    ReconPtr2 += ReconPixelsPerLine;
    DctInputPtr += BLOCK_HEIGHT_WIDTH;
  }
}

static unsigned char TokenizeDctValue (ogg_int16_t DataValue,
                                       ogg_uint32_t * TokenListPtr ){
  unsigned char tokens_added = 0;
  ogg_uint32_t AbsDataVal = abs( (ogg_int32_t)DataValue );

  /* Values are tokenised as category value and a number of additional
     bits that define the position within the category.  */

  if ( DataValue == 0 ) return 0;

  if ( AbsDataVal == 1 ){
    if ( DataValue == 1 )
      TokenListPtr[0] = ONE_TOKEN;
    else
      TokenListPtr[0] = MINUS_ONE_TOKEN;
    tokens_added = 1;
  } else if ( AbsDataVal == 2 ) {
    if ( DataValue == 2 )
      TokenListPtr[0] = TWO_TOKEN;
    else
      TokenListPtr[0] = MINUS_TWO_TOKEN;
    tokens_added = 1;
  } else if ( AbsDataVal <= MAX_SINGLE_TOKEN_VALUE ) {
    TokenListPtr[0] = LOW_VAL_TOKENS + (AbsDataVal - DCT_VAL_CAT2_MIN);
    if ( DataValue > 0 )
      TokenListPtr[1] = 0;
    else
      TokenListPtr[1] = 1;
    tokens_added = 2;
  } else if ( AbsDataVal <= 8 ) {
    /* Bit 1 determines sign, Bit 0 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY3;
    if ( DataValue > 0 )
      TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT3_MIN);
    else
      TokenListPtr[1] = (0x02) + (AbsDataVal - DCT_VAL_CAT3_MIN);
    tokens_added = 2;
  } else if ( AbsDataVal <= 12 ) {
    /* Bit 2 determines sign, Bit 0-2 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY4;
    if ( DataValue > 0 )
      TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT4_MIN);
    else
      TokenListPtr[1] = (0x04) + (AbsDataVal - DCT_VAL_CAT4_MIN);
    tokens_added = 2;
  } else if ( AbsDataVal <= 20 ) {
    /* Bit 3 determines sign, Bit 0-2 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY5;
    if ( DataValue > 0 )
      TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT5_MIN);
    else
      TokenListPtr[1] = (0x08) + (AbsDataVal - DCT_VAL_CAT5_MIN);
    tokens_added = 2;
  } else if ( AbsDataVal <= 36 ) {
    /* Bit 4 determines sign, Bit 0-3 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY6;
    if ( DataValue > 0 )
      TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT6_MIN);
    else
      TokenListPtr[1] = (0x010) + (AbsDataVal - DCT_VAL_CAT6_MIN);
    tokens_added = 2;
  } else if ( AbsDataVal <= 68 ) {
    /* Bit 5 determines sign, Bit 0-4 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY7;
    if ( DataValue > 0 )
      TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT7_MIN);
    else
      TokenListPtr[1] = (0x20) + (AbsDataVal - DCT_VAL_CAT7_MIN);
    tokens_added = 2;
  } else if ( AbsDataVal <= 511 ) {
    /* Bit 9 determines sign, Bit 0-8 the value */
    TokenListPtr[0] = DCT_VAL_CATEGORY8;
    if ( DataValue > 0 )
      TokenListPtr[1] = (AbsDataVal - DCT_VAL_CAT8_MIN);
    else
      TokenListPtr[1] = (0x200) + (AbsDataVal - DCT_VAL_CAT8_MIN);
    tokens_added = 2;
  } else {
    TokenListPtr[0] = DCT_VAL_CATEGORY8;
    if ( DataValue > 0 )
      TokenListPtr[1] = (511 - DCT_VAL_CAT8_MIN);
    else
      TokenListPtr[1] = (0x200) + (511 - DCT_VAL_CAT8_MIN);
    tokens_added = 2;
  }

  /* Return the total number of tokens added */
  return tokens_added;
}

static unsigned char TokenizeDctRunValue (unsigned char RunLength,
                                          ogg_int16_t DataValue,
                                          ogg_uint32_t * TokenListPtr ){
  unsigned char tokens_added = 0;
  ogg_uint32_t AbsDataVal = abs( (ogg_int32_t)DataValue );

  /* Values are tokenised as category value and a number of additional
     bits  that define the category.  */
  if ( DataValue == 0 ) return 0;
  if ( AbsDataVal == 1 ) {
    /* Zero runs of 1-5 */
    if ( RunLength <= 5 ) {
      TokenListPtr[0] = DCT_RUN_CATEGORY1 + (RunLength - 1);
      if ( DataValue > 0 )
        TokenListPtr[1] = 0;
      else
        TokenListPtr[1] = 1;
    } else if ( RunLength <= 9 ) {
      /* Zero runs of 6-9 */
      TokenListPtr[0] = DCT_RUN_CATEGORY1B;
      if ( DataValue > 0 )
        TokenListPtr[1] = (RunLength - 6);
      else
        TokenListPtr[1] = 0x04 + (RunLength - 6);
    } else {
      /* Zero runs of 10-17 */
      TokenListPtr[0] = DCT_RUN_CATEGORY1C;
      if ( DataValue > 0 )
        TokenListPtr[1] = (RunLength - 10);
      else
        TokenListPtr[1] = 0x08 + (RunLength - 10);
    }
    tokens_added = 2;
  } else if ( AbsDataVal <= 3 ) {
    if ( RunLength == 1 ) {
      TokenListPtr[0] = DCT_RUN_CATEGORY2;

      /* Extra bits token bit 1 indicates sign, bit 0 indicates value */
      if ( DataValue > 0 )
        TokenListPtr[1] = (AbsDataVal - 2);
      else
        TokenListPtr[1] = (0x02) + (AbsDataVal - 2);
      tokens_added = 2;
    }else{
      TokenListPtr[0] = DCT_RUN_CATEGORY2 + 1;

      /* Extra bits token. */
      /* bit 2 indicates sign, bit 1 indicates value, bit 0 indicates
         run length */
      if ( DataValue > 0 )
        TokenListPtr[1] = ((AbsDataVal - 2) << 1) + (RunLength - 2);
      else
        TokenListPtr[1] = (0x04) + ((AbsDataVal - 2) << 1) + (RunLength - 2);
      tokens_added = 2;
    }
  } else  {
    tokens_added = 2;  /* ERROR */
    /*IssueWarning( "Bad Input to TokenizeDctRunValue" );*/
  }

  /* Return the total number of tokens added */
  return tokens_added;
}

static unsigned char TokenizeDctBlock (ogg_int16_t * RawData,
                                       ogg_uint32_t * TokenListPtr ) {
  ogg_uint32_t i;
  unsigned char  run_count;
  unsigned char  token_count = 0;     /* Number of tokens crated. */
  ogg_uint32_t AbsData;


  /* Tokenize the block */
  for( i = 0; i < BLOCK_SIZE; i++ ){
    run_count = 0;

    /* Look for a zero run.  */
    /* NOTE the use of & instead of && which is faster (and
       equivalent) in this instance. */
    /* NO, NO IT ISN'T --Monty */
    while( (i < BLOCK_SIZE) && (!RawData[i]) ){
      run_count++;
      i++;
    }

    /* If we have reached the end of the block then code EOB */
    if ( i == BLOCK_SIZE ){
      TokenListPtr[token_count] = DCT_EOB_TOKEN;
      token_count++;
    }else{
      /* If we have a short zero run followed by a low data value code
         the two as a composite token. */
      if ( run_count ){
        AbsData = abs(RawData[i]);

        if ( ((AbsData == 1) && (run_count <= 17)) ||
             ((AbsData <= 3) && (run_count <= 3)) ) {
          /* Tokenise the run and subsequent value combination value */
          token_count += TokenizeDctRunValue( run_count,
                                              RawData[i],
                                              &TokenListPtr[token_count] );
        }else{

        /* Else if we have a long non-EOB run or a run followed by a
           value token > MAX_RUN_VAL then code the run and token
           seperately */
          if ( run_count <= 8 )
            TokenListPtr[token_count] = DCT_SHORT_ZRL_TOKEN;
          else
            TokenListPtr[token_count] = DCT_ZRL_TOKEN;

          token_count++;
          TokenListPtr[token_count] = run_count - 1;
          token_count++;

          /* Now tokenize the value */
          token_count += TokenizeDctValue( RawData[i],
                                           &TokenListPtr[token_count] );
        }
      }else{
        /* Else there was NO zero run. */
        /* Tokenise the value  */
        token_count += TokenizeDctValue( RawData[i],
                                         &TokenListPtr[token_count] );
      }
    }
  }

  /* Return the total number of tokens (including additional bits
     tokens) used. */
  return token_count;
}

ogg_uint32_t DPCMTokenizeBlock (CP_INSTANCE *cpi,
                                ogg_int32_t FragIndex){
  ogg_uint32_t  token_count;

  if ( GetFrameType(&cpi->pb) == BASE_FRAME ){
    /* Key frame so code block in INTRA mode. */
    cpi->pb.CodingMode = CODE_INTRA;
  }else{
    /* Get Motion vector and mode for this block. */
    cpi->pb.CodingMode = cpi->pb.FragCodingMethod[FragIndex];
  }

  /* Tokenise the dct data. */
  token_count = TokenizeDctBlock( cpi->pb.QFragData[FragIndex],
                                  cpi->pb.TokenList[FragIndex] );

  cpi->FragTokenCounts[FragIndex] = token_count;
  cpi->TotTokenCount += token_count;

  /* Return number of pixels coded (i.e. 8x8). */
  return BLOCK_SIZE;
}

static int AllZeroDctData( Q_LIST_ENTRY * QuantList ){
  ogg_uint32_t i;

  for ( i = 0; i < 64; i ++ )
    if ( QuantList[i] != 0 )
      return 0;

  return 1;
}

static void MotionBlockDifference (CP_INSTANCE * cpi, unsigned char * FiltPtr,
                            ogg_int16_t *DctInputPtr, ogg_int32_t MvDevisor,
                            unsigned char* old_ptr1, unsigned char* new_ptr1,
                            ogg_uint32_t FragIndex,ogg_uint32_t PixelsPerLine,
                            ogg_uint32_t ReconPixelsPerLine) {

  ogg_int32_t MvShift;
  ogg_int32_t MvModMask;
  ogg_int32_t  AbsRefOffset;
  ogg_int32_t  AbsXOffset;
  ogg_int32_t  AbsYOffset;
  ogg_int32_t  MVOffset;        /* Baseline motion vector offset */
  ogg_int32_t  ReconPtr2Offset; /* Offset for second reconstruction in
                                   half pixel MC */
  unsigned char  *ReconPtr1;    /* DCT reconstructed image pointers */
  unsigned char  *ReconPtr2;    /* Pointer used in half pixel MC */


  switch(MvDevisor) {
  case 2:
    MvShift = 1;
    MvModMask = 1;
    break;
  case 4:
    MvShift = 2;
    MvModMask = 3;
    break;
  default:
    break;
  }

  cpi->MVector.x = cpi->pb.FragMVect[FragIndex].x;
  cpi->MVector.y = cpi->pb.FragMVect[FragIndex].y;

  /* Set up the baseline offset for the motion vector. */
  MVOffset = ((cpi->MVector.y / MvDevisor) * ReconPixelsPerLine) +
    (cpi->MVector.x / MvDevisor);

  /* Work out the offset of the second reference position for 1/2
     pixel interpolation.  For the U and V planes the MV specifies 1/4
     pixel accuracy. This is adjusted to 1/2 pixel as follows ( 0->0,
     1/4->1/2, 1/2->1/2, 3/4->1/2 ). */
  ReconPtr2Offset = 0;
  AbsXOffset = cpi->MVector.x % MvDevisor;
  AbsYOffset = cpi->MVector.y % MvDevisor;

  if ( AbsXOffset ) {
    if ( cpi->MVector.x > 0 )
      ReconPtr2Offset += 1;
    else
      ReconPtr2Offset -= 1;
  }

  if ( AbsYOffset ) {
    if ( cpi->MVector.y > 0 )
      ReconPtr2Offset += ReconPixelsPerLine;
    else
      ReconPtr2Offset -= ReconPixelsPerLine;
  }

  if ( cpi->pb.CodingMode==CODE_GOLDEN_MV ) {
    ReconPtr1 = &cpi->
      pb.GoldenFrame[cpi->pb.recon_pixel_index_table[FragIndex]];
  } else {
    ReconPtr1 = &cpi->
      pb.LastFrameRecon[cpi->pb.recon_pixel_index_table[FragIndex]];
  }

  ReconPtr1 += MVOffset;
  ReconPtr2 =  ReconPtr1 + ReconPtr2Offset;

  AbsRefOffset = abs((int)(ReconPtr1 - ReconPtr2));

  /* Is the MV offset exactly pixel alligned */
  if ( AbsRefOffset == 0 ){
    Sub8( FiltPtr, ReconPtr1, DctInputPtr, old_ptr1, new_ptr1,
               PixelsPerLine, ReconPixelsPerLine );
  } else {
    /* Fractional pixel MVs. */
    /* Note that we only use two pixel values even for the diagonal */
    Sub8Av2(FiltPtr, ReconPtr1,ReconPtr2,DctInputPtr, old_ptr1,
                 new_ptr1, PixelsPerLine, ReconPixelsPerLine );
  }
}

void TransformQuantizeBlock (CP_INSTANCE *cpi, ogg_int32_t FragIndex,
                             ogg_uint32_t PixelsPerLine ) {
  unsigned char *new_ptr1;    /* Pointers into current frame */
  unsigned char *old_ptr1;    /* Pointers into old frame */
  unsigned char *FiltPtr;     /* Pointers to srf filtered pixels */
  ogg_int16_t   *DctInputPtr; /* Pointer into buffer containing input to DCT */
  int LeftEdge;               /* Flag if block at left edge of component */
  ogg_uint32_t  ReconPixelsPerLine; /* Line length for recon buffers. */

  unsigned char   *ReconPtr1;   /* DCT reconstructed image pointers */
  ogg_int32_t   MvDevisor;      /* Defines MV resolution (2 = 1/2
                                   pixel for Y or 4 = 1/4 for UV) */

  new_ptr1 = &cpi->yuv1ptr[cpi->pb.pixel_index_table[FragIndex]];
  old_ptr1 = &cpi->yuv0ptr[cpi->pb.pixel_index_table[FragIndex]];
  DctInputPtr   = cpi->DCTDataBuffer;

  /* Set plane specific values */
  if (FragIndex < (ogg_int32_t)cpi->pb.YPlaneFragments){
    ReconPixelsPerLine = cpi->pb.YStride;
    MvDevisor = 2;                  /* 1/2 pixel accuracy in Y */
  }else{
    ReconPixelsPerLine = cpi->pb.UVStride;
    MvDevisor = 4;                  /* UV planes at 1/2 resolution of Y */
  }

  /* adjusted / filtered pointers */
  FiltPtr = &cpi->ConvDestBuffer[cpi->pb.pixel_index_table[FragIndex]];

  if ( GetFrameType(&cpi->pb) == BASE_FRAME ) {
    /* Key frame so code block in INTRA mode. */
    cpi->pb.CodingMode = CODE_INTRA;
  }else{
    /* Get Motion vector and mode for this block. */
    cpi->pb.CodingMode = cpi->pb.FragCodingMethod[FragIndex];
  }

  /* Selection of Quantiser matirx and set other plane related values. */
  if ( FragIndex < (ogg_int32_t)cpi->pb.YPlaneFragments ){
    LeftEdge = !(FragIndex%cpi->pb.HFragments);

    /* Select the approrpriate Y quantiser matrix */
    if ( cpi->pb.CodingMode == CODE_INTRA )
      select_Y_quantiser(&cpi->pb);
    else
      select_Inter_quantiser(&cpi->pb);
  }else{
    LeftEdge = !((FragIndex-cpi->pb.YPlaneFragments)%(cpi->pb.HFragments>>1));

    /* Select the approrpriate UV quantiser matrix */
    if ( cpi->pb.CodingMode == CODE_INTRA )
      select_UV_quantiser(&cpi->pb);
    else
      select_Inter_quantiser(&cpi->pb);
  }

  if ( ModeUsesMC[cpi->pb.CodingMode] ){

    MotionBlockDifference(cpi, FiltPtr, DctInputPtr, MvDevisor,
                          old_ptr1, new_ptr1, FragIndex, PixelsPerLine,
                          ReconPixelsPerLine);

  } else if ( (cpi->pb.CodingMode==CODE_INTER_NO_MV ) ||
              ( cpi->pb.CodingMode==CODE_USING_GOLDEN ) ) {
    if ( cpi->pb.CodingMode==CODE_INTER_NO_MV ) {
      ReconPtr1 = &cpi->
        pb.LastFrameRecon[cpi->pb.recon_pixel_index_table[FragIndex]];
    } else {
      ReconPtr1 = &cpi->
        pb.GoldenFrame[cpi->pb.recon_pixel_index_table[FragIndex]];
    }

    Sub8( FiltPtr, ReconPtr1, DctInputPtr, old_ptr1, new_ptr1,
               PixelsPerLine, ReconPixelsPerLine );
  } else if ( cpi->pb.CodingMode==CODE_INTRA ) {
    Sub8_128(FiltPtr, DctInputPtr, old_ptr1, new_ptr1, PixelsPerLine);

  }

  /* Proceed to encode the data into the encode buffer if the encoder
     is enabled. */
  /* Perform a 2D DCT transform on the data. */
  fdct_short( cpi->DCTDataBuffer, cpi->DCT_codes );

  /* Quantize that transform data. */
  quantize ( &cpi->pb, cpi->DCT_codes, cpi->pb.QFragData[FragIndex] );

  if ( (cpi->pb.CodingMode == CODE_INTER_NO_MV) &&
       ( AllZeroDctData(cpi->pb.QFragData[FragIndex]) ) ) {
    cpi->pb.display_fragments[FragIndex] = 0;
  }

}
