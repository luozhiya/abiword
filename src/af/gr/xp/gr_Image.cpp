/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  
 * 02111-1307, USA.
 */

#include <string.h>

#include "gr_Image.h"
#include "ut_bytebuf.h"
#include "ut_svg.h"
#include "ut_assert.h"

GR_Image::GR_Image()
  : m_szName(""), m_iDisplayWidth(0), m_iDisplayHeight(0)
{
}

GR_Image::~GR_Image()
{
  DestroyOutline();
}

void GR_Image::getName(char* p) const
{
	UT_ASSERT(p);
	
	strcpy(p, m_szName.c_str());
}

void GR_Image::getName ( UT_String & copy ) const
{
  // assign
  copy = m_szName;
}

/*!
 * Scale our image to rectangle given by rec. The dimensions of rec
 * are calculated in logical units.
 * Overriden by platform implementation if needed. Default is to set 
 * display size.
 */
void GR_Image::scaleImageTo(GR_Graphics * pG, const UT_Rect & rec)
{
	setDisplaySize(pG->tdu(rec.width), pG->tdu(rec.height));
}

void GR_Image::setName ( const char * name )
{
  m_szName = name;
}

/*!
 * Return the distance from the left side of the image that is "pad" distance
 * to the nearest point in the transparent outline from the line segment
 * start at Y and running for distance Height below it.
 * All distances are in logical units.
 *                -----------------
 *                |               |
 *                |      *        |
 *                |    *****      |
 *             ||||||   ***       |
 *                | |    **       |
 *                |---------------|
 *                | |
 *                | |
 *
 * This case would give a -ve distance.
 */
UT_sint32 GR_Image::GetOffsetFromLeft(GR_Graphics * pG, UT_sint32 pad, UT_sint32 yTop, UT_sint32 height)
{
  return pad;
}


/*!
 * Return the distance from the right side of the image that is "pad" distance
 * to the nearest point in the transparent outline from the line segment
 * start at Y and running for distance Height below it.
 * All distances are in logical units.
 *                -----------------
 *                |               |
 *                |      *        |
 *                |    *****      |
 *                |     ***       |
 *                |      **       |
 *                |---------------|
 */
UT_sint32 GR_Image::GetOffsetFromRight(GR_Graphics * pG, UT_sint32 pad, UT_sint32 yTop, UT_sint32 height)
{
  return pad;
}

/*!
 * Generate an outline of an image with transparency. This is a collection
 * of (x,y) points marking the first non-transparent point from the left
 * and right side of the image.
 * This outline is used by GetOffsetFromLeft and facitates "tight" 
 * text wrapping
 * around objects.
 */
void GR_Image::GenerateOutline(void)
{
  DestroyOutline();
  UT_sint32 width = getDisplayWidth();
  UT_sint32 height = getDisplayHeight();
  UT_sint32 i,j=0;
  //
  // Generate from left
  //
  for(i=0; i< height;i++)
  {
    for(j =0; j< width;j++)
    {
      if(!isTransparentAt(i,j))
      {
	break;
      }
    }
    if( j < width)
    {
      GR_Image_Point * pXY = new GR_Image_Point();
      pXY->m_iX = i;
      pXY->m_iY = j;
      m_vecOutLine.addItem(pXY);
    }
  }
  //
  // Generate from Right
  //
  for(i=0; i< height;i++)
  {
    for(j =width-1; j>= 0;j--)
    {
      if(!isTransparentAt(i,j))
      {
	break;
      }
    }
    if( j >= 0)
    {
      GR_Image_Point * pXY = new GR_Image_Point();
      pXY->m_iX = i;
      pXY->m_iY = j;
      m_vecOutLine.addItem(pXY);
    }
  }
}

/*!
 * Destroy the outline
 */
void GR_Image::DestroyOutline(void)
{
  UT_VECTOR_PURGEALL(GR_Image_Point *, m_vecOutLine);
}

void GR_Image::setName ( const UT_String & name )
{
  m_szName = name;
}

GR_Image::GRType GR_Image::getBufferType(const UT_ByteBuf * pBB)
{
   const char * buf = reinterpret_cast<const char*>(pBB->getPointer(0));
   UT_uint32 len = pBB->getLength();

   if (len < 6) return GR_Image::GRT_Unknown;

   char * comp1 = "\211PNG";
   char * comp2 = "<89>PNG";
   if (!(strncmp(static_cast<const char*>(buf), comp1, 4)) || !(strncmp(static_cast<const char*>(buf), comp2, 6)))
     return GR_Image::GRT_Raster;

   if (UT_SVG_recognizeContent (buf,len))
     return GR_Image::GRT_Vector;

   return GR_Image::GRT_Unknown;
}

void GR_Image::setDisplaySize(UT_sint32 iDisplayWidth, UT_sint32 iDisplayHeight) 
{ 
  m_iDisplayWidth = iDisplayWidth; 
  m_iDisplayHeight = iDisplayHeight; 
}
	
UT_sint32 GR_Image::getDisplayWidth(void) const 
{ 
  return m_iDisplayWidth; 
}

UT_sint32 GR_Image::getDisplayHeight(void) const 
{ 
  return m_iDisplayHeight; 
}

bool GR_Image::convertToBuffer(UT_ByteBuf** ppBB) const 
{ 
  // default no impl
  return false; 
}

bool GR_Image::convertFromBuffer(const UT_ByteBuf* pBB, UT_sint32 iWidth, UT_sint32 iHeight) 
{ 
  // default no impl
  UT_ASSERT_NOT_REACHED ();
  return false; 
}

GR_Image::GRType GR_Image::getType() const
{ 
//
// While this is technically the right thing to do it screws up printing on Windows and Gnome
//
//  return GRT_Unknown;
//
// FIXME: Subclasses should ensure this works.
	return GRT_Raster;
}

bool GR_Image::render(GR_Graphics *pGR, UT_sint32 iDisplayWidth, UT_sint32 iDisplayHeight)
{ 
  UT_ASSERT_NOT_REACHED ();
  return false; 
}
