/****************************************************************************
**
** This file is part of the LibreCAD project, a 2D CAD program
**
** Copyright (C) 2010 R. van Twisk (librecad@rvt.dds.nl)
** Copyright (C) 2001-2003 RibbonSoft. All rights reserved.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU General Public License version 2 as published by the Free Software
** Foundation and appearing in the file gpl-2.0.txt included in the
** packaging of this file.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**
** This copyright notice MUST APPEAR in all copies of the script!
**
**********************************************************************/

#include <iostream>
#include <cmath>
#include "rs_dimordinate.h"
#include "rs_line.h"

#include "rs_graphic.h"
#include "rs_units.h"
#include "rs_constructionline.h"
#include "rs_math.h"
#include "rs_debug.h"

RS_DimOrdinateData::RS_DimOrdinateData():
    originPoint(false),
	extensionPoint2(false)
{}

/**
 * Constructor with initialisation.
 *
    * @para originPoint Definition point. Startpoint of the
 *          origin extension line.
 */
RS_DimOrdinateData::RS_DimOrdinateData(const RS_Vector& _extensionPoint1,
				  const RS_Vector& _extensionPoint2):
    originPoint(_extensionPoint1)
	,extensionPoint2(_extensionPoint2)
{
}

std::ostream& operator << (std::ostream& os,
                                  const RS_DimOrdinateData& dd) {
    os << "(" << dd.originPoint << "/" << dd.originPoint << ")";
	return os;
}

/**
 * Constructor.
 *
 * @para parent Parent Entity Container.
 * @para d Common dimension geometrical data.
 * @para ed Extended geometrical data for ordinate dimension.
 */
RS_DimOrdinate::RS_DimOrdinate(RS_EntityContainer* parent,
                             const RS_DimensionData& d,
                             const RS_DimOrdinateData& ed)
        : RS_Dimension(parent, d), edata(ed) {

    calculateBorders();
}



/**
 * Sets a new text. The entities representing the
 * text are updated.
 */
//void RS_DimOrdinate::setText(const QString& t) {
//    data.text = t;
//    update();
//}

RS_Entity* RS_DimOrdinate::clone() const{
    RS_DimOrdinate* d = new RS_DimOrdinate(*this);
	d->setOwner(isOwner());
	d->initId();
	d->detach();
	return d;
}

RS_VectorSolutions RS_DimOrdinate::getRefPoints() const
{
        return RS_VectorSolutions({edata.originPoint, edata.extensionPoint2,
												data.definitionPoint, data.middleOfText});
}

/**
 * @return Automatically creted label for the default
 * measurement of this dimension.
 */
QString RS_DimOrdinate::getMeasuredLabel() {
    double dist = edata.originPoint.distanceTo(edata.extensionPoint2) * getGeneralFactor();

    RS_Graphic* graphic = getGraphic();
    QString ret;
    if (graphic) {
        int dimlunit = getGraphicVariableInt("$DIMLUNIT", 2);
        int dimdec = getGraphicVariableInt("$DIMDEC", 4);
        int dimzin = getGraphicVariableInt("$DIMZIN", 1);
        RS2::LinearFormat format = graphic->getLinearFormat(dimlunit);

        ret = RS_Units::formatLinear(dist, RS2::None, format, dimdec);
        if (format == RS2::Decimal)
            ret = stripZerosLinear(ret, dimzin);
        //verify if units are decimal and comma separator
        if (format == RS2::Decimal || format == RS2::ArchitecturalMetric){
            if (getGraphicVariableInt("$DIMDSEP", 0) == 44)
                ret.replace(QChar('.'), QChar(','));
        }
    }
    else {
        ret = QString("%1").arg(dist);
    }
    return ret;
}


RS_DimOrdinateData const& RS_DimOrdinate::getEData() const {
	return edata;
}

RS_Vector const& RS_DimOrdinate::getOriginPoint() const {
    return edata.originPoint;
}

//TODO remove def
/*
RS_Vector const& RS_DimOrdinate::getExtensionPoint2() const {
	return edata.extensionPoint2;
}
*/

/**
 * Updates the sub entities of this dimension. Called when the
 * text or the position, alignment, .. changes.
 *
 * @param autoText Automatically reposition the text label
 */
void RS_DimOrdinate::updateDim(bool autoText) {

    RS_DEBUG->print("RS_DimOrdinate::update");

    clear();

    if (isUndone()) {
        return;
    }

    // general scale (DIMSCALE)
    double dimscale = getGeneralScale();
    // distance from entities (DIMEXO)
    double dimexo = getExtensionLineOffset()*dimscale;
    // definition line definition (DIMEXE)
    double dimexe = getExtensionLineExtension()*dimscale;
    // text height (DIMTXT)
    //double dimtxt = getTextHeight();
    // text distance to line (DIMGAP)
    //double dimgap = getDimensionLineGap();

    double extAngle,extLength;
    // Angle from extension endpoints towards dimension line
    // extension lines length
    double unconstrainedAngle = edata.originPoint.angleTo(data.definitionPoint);
    //TODO remove this debug print line
    qDebug( "Angle: %f, Origin: %f, Def: %f", unconstrainedAngle, edata.originPoint, data.definitionPoint );
    if (unconstrainedAngle < M_PI_4){
        extAngle = 0;
        extLength = abs(edata.originPoint.x - data.definitionPoint.x);
    }else if (unconstrainedAngle < 3 * M_PI_4){
        extAngle = M_PI_2;
        extLength = abs(edata.originPoint.y - data.definitionPoint.y);
    }else if (unconstrainedAngle < 5 * M_PI_4){
        extAngle = M_PI;
        extLength = abs(edata.originPoint.x - data.definitionPoint.x);
    }else if (unconstrainedAngle < 7 * M_PI_4){
        extAngle = 3 * M_PI_2;
        extLength = abs(edata.originPoint.y - data.definitionPoint.y);
    }else{
        extAngle = 0;
        extLength = abs(edata.originPoint.x - data.definitionPoint.x);
    }

    if (getFixedLengthOn()){
        double dimfxl = getFixedLength();
        if (extLength-dimexo > dimfxl)
            dimexo =  extLength - dimfxl;
    }

	RS_Vector v1 = RS_Vector::polar(dimexo, extAngle);
	RS_Vector v2 = RS_Vector::polar(dimexe, extAngle);
	RS_Vector e1 = RS_Vector::polar(1.0, extAngle);

    RS_Pen pen(getExtensionLineColor(),
           getExtensionLineWidth(),
           RS2::LineByBlock);

    // Extension line 1:
	RS_Line* line = new RS_Line{this,
            edata.originPoint + v1,
            edata.originPoint + e1*extLength + v2};
    //line->setLayerToActive();
    //line->setPenToActive();
//    line->setPen(RS_Pen(RS2::FlagInvalid));
    line->setPen(pen);
	line->setLayer(nullptr);
    addEntity(line);

    //TODO remove extension line 2 definition
    /*
    // Extension line 2:
	line = new RS_Line{this,
			edata.extensionPoint2 + v1,
			edata.extensionPoint2 + e1*extLength + v2};
    //line->setLayerToActive();
    //line->setPenToActive();
//    line->setPen(RS_Pen(RS2::FlagInvalid));
    line->setPen(pen);
	line->setLayer(nullptr);
    addEntity(line);
    */

    // Dimension line:
    /*updateCreateDimensionLine(edata.originPoint + e1*extLength,
                              edata.originPoint + 5.0 + e1*extLength,
                                                          true, true, autoText);
    */
    calculateBorders();
}


void RS_DimOrdinate::updateDimPoint(){
    // temporary construction line
	RS_ConstructionLine tmpLine( nullptr,
        RS_ConstructionLineData(edata.originPoint, edata.extensionPoint2));

	RS_Vector tmpP1 = tmpLine.getNearestPointOnEntity(data.definitionPoint);
	data.definitionPoint += edata.extensionPoint2 - tmpP1;
}


bool RS_DimOrdinate::hasEndpointsWithinWindow(const RS_Vector& v1, const RS_Vector& v2) {
        return (edata.originPoint.isInWindow(v1, v2) ||
                edata.extensionPoint2.isInWindow(v1, v2));
}


void RS_DimOrdinate::move(const RS_Vector& offset) {
    RS_Dimension::move(offset);

    edata.originPoint.move(offset);
    edata.extensionPoint2.move(offset);
    update();
}



void RS_DimOrdinate::rotate(const RS_Vector& center, const double& angle) {
   rotate(center,RS_Vector(angle));
}


void RS_DimOrdinate::rotate(const RS_Vector& center, const RS_Vector& angleVector) {
    RS_Dimension::rotate(center, angleVector);
    edata.originPoint.rotate(center, angleVector);
    edata.extensionPoint2.rotate(center, angleVector);
    update();
}

void RS_DimOrdinate::scale(const RS_Vector& center, const RS_Vector& factor) {
    RS_Dimension::scale(center, factor);

    edata.originPoint.scale(center, factor);
    edata.extensionPoint2.scale(center, factor);
    update();
}


void RS_DimOrdinate::mirror(const RS_Vector& axisPoint1, const RS_Vector& axisPoint2) {
    RS_Dimension::mirror(axisPoint1, axisPoint2);

    edata.originPoint.mirror(axisPoint1, axisPoint2);
    edata.extensionPoint2.mirror(axisPoint1, axisPoint2);
    update();
}


void RS_DimOrdinate::stretch(const RS_Vector& firstCorner,
                            const RS_Vector& secondCorner,
                            const RS_Vector& offset) {

    //e->calculateBorders();
    if (getMin().isInWindow(firstCorner, secondCorner) &&
            getMax().isInWindow(firstCorner, secondCorner)) {

        move(offset);
    }
        else {
                //RS_Vector v = data.definitionPoint - edata.extensionPoint2;
				double len = edata.extensionPoint2.distanceTo(data.definitionPoint);
                double ang1 = edata.originPoint.angleTo(edata.extensionPoint2)
							 + M_PI_2;

        if (edata.originPoint.isInWindow(firstCorner,
                                      secondCorner)) {
                edata.originPoint.move(offset);
        }
        if (edata.extensionPoint2.isInWindow(firstCorner,
                                      secondCorner)) {
                edata.extensionPoint2.move(offset);
        }

                double ang2 = edata.originPoint.angleTo(edata.extensionPoint2)
							 + M_PI_2;

                double diff = RS_Math::getAngleDifference(ang1, ang2);
                if (diff>M_PI) {
                        diff-=2*M_PI;
                }

				if (fabs(diff)>M_PI_2) {
                        ang2 = RS_Math::correctAngle(ang2+M_PI);
                }

				RS_Vector v = RS_Vector::polar(len, ang2);
				data.definitionPoint = edata.extensionPoint2 + v;
        }
        updateDim(true);
}



void RS_DimOrdinate::moveRef(const RS_Vector& ref, const RS_Vector& offset) {

	if (ref.distanceTo(data.definitionPoint)<1.0e-4) {
				RS_ConstructionLine l(nullptr,
                        RS_ConstructionLineData(edata.originPoint,
                                edata.extensionPoint2));
				double d = l.getDistanceToPoint(data.definitionPoint+offset);
				double a = edata.extensionPoint2.angleTo(data.definitionPoint);
                double ad = RS_Math::getAngleDifference(a,
						edata.extensionPoint2.angleTo(data.definitionPoint+offset));

				if (fabs(ad)>M_PI_2 && fabs(ad)<3.0/2.0*M_PI) {
                        a = RS_Math::correctAngle(a+M_PI);
                }

				RS_Vector v = RS_Vector::polar(d, a);
		data.definitionPoint = edata.extensionPoint2 + v;
                updateDim(true);
    }
        else if (ref.distanceTo(data.middleOfText)<1.0e-4) {
        data.middleOfText.move(offset);
                updateDim(false);
    }
        else if (ref.distanceTo(edata.originPoint)<1.0e-4) {
                double a1 = edata.extensionPoint2.angleTo(edata.originPoint);
                double a2 = edata.extensionPoint2.angleTo(edata.originPoint+offset);
                double d1 = edata.extensionPoint2.distanceTo(edata.originPoint);
                double d2 = edata.extensionPoint2.distanceTo(edata.originPoint+offset);
                rotate(edata.extensionPoint2, a2-a1);
                if (fabs(d1)>1.0e-4) {
                        scale(edata.extensionPoint2, RS_Vector(d2/d1, d2/d1));
                }
                updateDim(true);
    }
        else if (ref.distanceTo(edata.extensionPoint2)<1.0e-4) {
                double a1 = edata.originPoint.angleTo(edata.extensionPoint2);
                double a2 = edata.originPoint.angleTo(edata.extensionPoint2+offset);
                double d1 = edata.originPoint.distanceTo(edata.extensionPoint2);
                double d2 = edata.originPoint.distanceTo(edata.extensionPoint2+offset);
                rotate(edata.originPoint, a2-a1);
                if (fabs(d1)>1.0e-4) {
                        scale(edata.originPoint, RS_Vector(d2/d1, d2/d1));
                }
                updateDim(true);
    }
}



/**
 * Dumps the point's data to stdout.
 */
std::ostream& operator << (std::ostream& os, const RS_DimOrdinate& d) {
    os << " DimOrdinate: " << d.getData() << "\n" << d.getEData() << "\n";
    return os;
}
