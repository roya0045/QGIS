/***************************************************************************
                         qgscurvepolygon.cpp
                         ---------------------
    begin                : August 2017
    copyright            : (C) 2017 by Martí Angelats i Ribera
    email                : marti dot angelats at psig dot cat
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgssurface.h"
#include "qgspoint.h"
#include "qgspolygon.h"
#include "qgsgeos.h"
#include <memory>

bool QgsSurface::isValid( QString &error, int flags ) const
{
  if ( flags == 0 && mHasCachedValidity )
  {
    // use cached validity results
    error = mValidityFailureReason;
    return error.isEmpty();
  }

  QgsGeos geos( this );
  bool res = geos.isValid( &error, flags & QgsGeometry::FlagAllowSelfTouchingHoles, nullptr );
  if ( flags == 0 )
  {
    mValidityFailureReason = !res ? error : QString();
    mHasCachedValidity = true;
  }
  return res;
}

void QgsSurface::clearCache() const
{
  mBoundingBox = QgsRectangle();
  mHasCachedValidity = false;
  mValidityFailureReason.clear();
  QgsAbstractGeometry::clearCache();
}
