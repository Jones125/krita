/*
 *  Copyright (c) 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_algebra_2d.h"

#include <QTransform>
#include <QPainterPath>
#include <kis_debug.h>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>

#include <array>
#include <QVector2D>
#include <QVector3D>

#include <config-gsl.h>

#ifdef HAVE_GSL
#include <gsl/gsl_multimin.h>
#endif /*HAVE_GSL*/

#include <Eigen/Eigenvalues>

#define SANITY_CHECKS

namespace KisAlgebra2D {

void adjustIfOnPolygonBoundary(const QPolygonF &poly, int polygonDirection, QPointF *pt)
{
    const int numPoints = poly.size();
    for (int i = 0; i < numPoints; i++) {
        int nextI = i + 1;
        if (nextI >= numPoints) {
            nextI = 0;
        }

        const QPointF &p0 = poly[i];
        const QPointF &p1 = poly[nextI];

        QPointF edge = p1 - p0;

        qreal cross = crossProduct(edge, *pt - p0)
            / (0.5 * edge.manhattanLength());

        if (cross < 1.0 &&
            isInRange(pt->x(), p0.x(), p1.x()) &&
            isInRange(pt->y(), p0.y(), p1.y())) {

            QPointF salt = 1.0e-3 * inwardUnitNormal(edge, polygonDirection);

            QPointF adjustedPoint = *pt + salt;

            // in case the polygon is self-intersecting, polygon direction
            // might not help
            if (kisDistanceToLine(adjustedPoint, QLineF(p0, p1)) < 1e-4) {
                adjustedPoint = *pt - salt;

#ifdef SANITY_CHECKS
                if (kisDistanceToLine(adjustedPoint, QLineF(p0, p1)) < 1e-4) {
                    dbgKrita << ppVar(*pt);
                    dbgKrita << ppVar(adjustedPoint);
                    dbgKrita << ppVar(QLineF(p0, p1));
                    dbgKrita << ppVar(salt);

                    dbgKrita << ppVar(poly.containsPoint(*pt, Qt::OddEvenFill));

                    dbgKrita << ppVar(kisDistanceToLine(*pt, QLineF(p0, p1)));
                    dbgKrita << ppVar(kisDistanceToLine(adjustedPoint, QLineF(p0, p1)));
                }

                *pt = adjustedPoint;

                KIS_ASSERT_RECOVER_NOOP(kisDistanceToLine(*pt, QLineF(p0, p1)) > 1e-4);
#endif /* SANITY_CHECKS */
            }
        }
    }
}

QPointF transformAsBase(const QPointF &pt, const QPointF &base1, const QPointF &base2) {
    qreal len1 = norm(base1);
    if (len1 < 1e-5) return pt;
    qreal sin1 = base1.y() / len1;
    qreal cos1 = base1.x() / len1;

    qreal len2 = norm(base2);
    if (len2 < 1e-5) return QPointF();
    qreal sin2 = base2.y() / len2;
    qreal cos2 = base2.x() / len2;

    qreal sinD = sin2 * cos1 - cos2 * sin1;
    qreal cosD = cos1 * cos2 + sin1 * sin2;
    qreal scaleD = len2 / len1;

    QPointF result;
    result.rx() = scaleD * (pt.x() * cosD - pt.y() * sinD);
    result.ry() = scaleD * (pt.x() * sinD + pt.y() * cosD);

    return result;
}

qreal angleBetweenVectors(const QPointF &v1, const QPointF &v2)
{
    qreal a1 = std::atan2(v1.y(), v1.x());
    qreal a2 = std::atan2(v2.y(), v2.x());

    return a2 - a1;
}

qreal directionBetweenPoints(const QPointF &p1, const QPointF &p2, qreal defaultAngle)
{
    if (fuzzyPointCompare(p1, p2)) {
        return defaultAngle;
    }

    const QVector2D diff(p2 - p1);
    return std::atan2(diff.y(), diff.x());
}

QPainterPath smallArrow()
{
    QPainterPath p;

    p.moveTo(5, 2);
    p.lineTo(-3, 8);
    p.lineTo(-5, 5);
    p.lineTo( 2, 0);
    p.lineTo(-5,-5);
    p.lineTo(-3,-8);
    p.lineTo( 5,-2);
    p.arcTo(QRectF(3, -2, 4, 4), 90, -180);

    return p;
}

template <class Point, class Rect>
inline Point ensureInRectImpl(Point pt, const Rect &bounds)
{
    if (pt.x() > bounds.right()) {
        pt.rx() = bounds.right();
    } else if (pt.x() < bounds.left()) {
        pt.rx() = bounds.left();
    }

    if (pt.y() > bounds.bottom()) {
        pt.ry() = bounds.bottom();
    } else if (pt.y() < bounds.top()) {
        pt.ry() = bounds.top();
    }

    return pt;
}

QPoint ensureInRect(QPoint pt, const QRect &bounds)
{
    return ensureInRectImpl(pt, bounds);
}

QPointF ensureInRect(QPointF pt, const QRectF &bounds)
{
    return ensureInRectImpl(pt, bounds);
}

bool intersectLineRect(QLineF &line, const QRect rect)
{
    QPointF pt1 = QPointF(), pt2 = QPointF();
    QPointF tmp;

    if (line.intersect(QLineF(rect.topLeft(), rect.topRight()), &tmp) != QLineF::NoIntersection) {
        if (tmp.x() >= rect.left() && tmp.x() <= rect.right()) {
            pt1 = tmp;
        }
    }

    if (line.intersect(QLineF(rect.topRight(), rect.bottomRight()), &tmp) != QLineF::NoIntersection) {
        if (tmp.y() >= rect.top() && tmp.y() <= rect.bottom()) {
            if (pt1.isNull()) pt1 = tmp;
            else pt2 = tmp;
        }
    }
    if (line.intersect(QLineF(rect.bottomRight(), rect.bottomLeft()), &tmp) != QLineF::NoIntersection) {
        if (tmp.x() >= rect.left() && tmp.x() <= rect.right()) {
            if (pt1.isNull()) pt1 = tmp;
            else pt2 = tmp;
        }
    }
    if (line.intersect(QLineF(rect.bottomLeft(), rect.topLeft()), &tmp) != QLineF::NoIntersection) {
        if (tmp.y() >= rect.top() && tmp.y() <= rect.bottom()) {
            if (pt1.isNull()) pt1 = tmp;
            else pt2 = tmp;
        }
    }

    if (pt1.isNull() || pt2.isNull()) return false;

    // Attempt to retain polarity of end points
    if ((line.x1() < line.x2()) != (pt1.x() > pt2.x()) || (line.y1() < line.y2()) != (pt1.y() > pt2.y())) {
        tmp = pt1;
        pt1 = pt2;
        pt2 = tmp;
    }

    line.setP1(pt1);
    line.setP2(pt2);

    return true;
}

    template <class Rect, class Point>
    QVector<Point> sampleRectWithPoints(const Rect &rect)
    {
        QVector<Point> points;

        Point m1 = 0.5 * (rect.topLeft() + rect.topRight());
        Point m2 = 0.5 * (rect.bottomLeft() + rect.bottomRight());

        points << rect.topLeft();
        points << m1;
        points << rect.topRight();

        points << 0.5 * (rect.topLeft() + rect.bottomLeft());
        points << 0.5 * (m1 + m2);
        points << 0.5 * (rect.topRight() + rect.bottomRight());

        points << rect.bottomLeft();
        points << m2;
        points << rect.bottomRight();

        return points;
    }

    QVector<QPoint> sampleRectWithPoints(const QRect &rect)
    {
        return sampleRectWithPoints<QRect, QPoint>(rect);
    }

    QVector<QPointF> sampleRectWithPoints(const QRectF &rect)
    {
        return sampleRectWithPoints<QRectF, QPointF>(rect);
    }


    template <class Rect, class Point, bool alignPixels>
    Rect approximateRectFromPointsImpl(const QVector<Point> &points)
    {
        using namespace boost::accumulators;
        accumulator_set<qreal, stats<tag::min, tag::max > > accX;
        accumulator_set<qreal, stats<tag::min, tag::max > > accY;

        Q_FOREACH (const Point &pt, points) {
            accX(pt.x());
            accY(pt.y());
        }

        Rect resultRect;

        if (alignPixels) {
            resultRect.setCoords(std::floor(min(accX)), std::floor(min(accY)),
                                 std::ceil(max(accX)), std::ceil(max(accY)));
        } else {
            resultRect.setCoords(min(accX), min(accY),
                                 max(accX), max(accY));
        }

        return resultRect;
    }

    QRect approximateRectFromPoints(const QVector<QPoint> &points)
    {
        return approximateRectFromPointsImpl<QRect, QPoint, true>(points);
    }

    QRectF approximateRectFromPoints(const QVector<QPointF> &points)
    {
        return approximateRectFromPointsImpl<QRectF, QPointF, false>(points);
    }

    QRect approximateRectWithPointTransform(const QRect &rect, std::function<QPointF(QPointF)> func)
    {
        QVector<QPoint> points = sampleRectWithPoints(rect);

        using namespace boost::accumulators;
        accumulator_set<qreal, stats<tag::min, tag::max > > accX;
        accumulator_set<qreal, stats<tag::min, tag::max > > accY;

        Q_FOREACH (const QPoint &pt, points) {
            QPointF dstPt = func(pt);

            accX(dstPt.x());
            accY(dstPt.y());
        }

        QRect resultRect;
        resultRect.setCoords(std::floor(min(accX)), std::floor(min(accY)),
                             std::ceil(max(accX)), std::ceil(max(accY)));

        return resultRect;
    }


QRectF cutOffRect(const QRectF &rc, const KisAlgebra2D::RightHalfPlane &p)
{
    QVector<QPointF> points;

    const QLineF cutLine = p.getLine();

    points << rc.topLeft();
    points << rc.topRight();
    points << rc.bottomRight();
    points << rc.bottomLeft();

    QPointF p1 = points[3];
    bool p1Valid = p.pos(p1) >= 0;

    QVector<QPointF> resultPoints;

    for (int i = 0; i < 4; i++) {
        const QPointF p2 = points[i];
        const bool p2Valid = p.pos(p2) >= 0;

        if (p1Valid != p2Valid) {
            QPointF intersection;
            cutLine.intersect(QLineF(p1, p2), &intersection);
            resultPoints << intersection;
        }

        if (p2Valid) {
            resultPoints << p2;
        }

        p1 = p2;
        p1Valid = p2Valid;
    }

    return approximateRectFromPoints(resultPoints);
}

int quadraticEquation(qreal a, qreal b, qreal c, qreal *x1, qreal *x2)
{
    int numSolutions = 0;

    const qreal D = pow2(b) - 4 * a * c;
    const qreal eps = 1e-14;

    if (qAbs(D) <= eps) {
        *x1 = -b / (2 * a);
        numSolutions = 1;
    } else if (D < 0) {
        return 0;
    } else {
        const qreal sqrt_D = std::sqrt(D);

        *x1 = (-b + sqrt_D) / (2 * a);
        *x2 = (-b - sqrt_D) / (2 * a);
        numSolutions = 2;
    }

    return numSolutions;
}

QVector<QPointF> intersectTwoCircles(const QPointF &center1, qreal r1,
                                     const QPointF &center2, qreal r2)
{
    QVector<QPointF> points;

    const QPointF diff = (center2 - center1);
    const QPointF c1;
    const QPointF c2 = diff;

    const qreal centerDistance = norm(diff);

    if (centerDistance > r1 + r2) return points;
    if (centerDistance < qAbs(r1 - r2)) return points;

    if (centerDistance < qAbs(r1 - r2) + 0.001) {
        dbgKrita << "Skipping intersection" << ppVar(center1) << ppVar(center2) << ppVar(r1) << ppVar(r2) << ppVar(centerDistance) << ppVar(qAbs(r1-r2));
        return points;
    }

    const qreal x_kp1 = diff.x();
    const qreal y_kp1 = diff.y();

    const qreal F2 =
        0.5 * (pow2(x_kp1) +
               pow2(y_kp1) + pow2(r1) - pow2(r2));

    const qreal eps = 1e-6;

    if (qAbs(diff.y()) < eps) {
        qreal x = F2 / diff.x();
        qreal y1, y2;
        int result = KisAlgebra2D::quadraticEquation(
            1, 0,
            pow2(x) - pow2(r2),
            &y1, &y2);

        KIS_SAFE_ASSERT_RECOVER(result > 0) { return points; }

        if (result == 1) {
            points << QPointF(x, y1);
        } else if (result == 2) {
            KisAlgebra2D::RightHalfPlane p(c1, c2);

            QPointF p1(x, y1);
            QPointF p2(x, y2);

            if (p.pos(p1) >= 0) {
                points << p1;
                points << p2;
            } else {
                points << p2;
                points << p1;
            }
        }
    } else {
        const qreal A = diff.x() / diff.y();
        const qreal C = F2 / diff.y();

        qreal x1, x2;
        int result = KisAlgebra2D::quadraticEquation(
            1 + pow2(A), -2 * A * C,
            pow2(C) - pow2(r1),
            &x1, &x2);

        KIS_SAFE_ASSERT_RECOVER(result > 0) { return points; }

        if (result == 1) {
            points << QPointF(x1, C - x1 * A);
        } else if (result == 2) {
            KisAlgebra2D::RightHalfPlane p(c1, c2);

            QPointF p1(x1, C - x1 * A);
            QPointF p2(x2, C - x2 * A);

            if (p.pos(p1) >= 0) {
                points << p1;
                points << p2;
            } else {
                points << p2;
                points << p1;
            }
        }
    }

    for (int i = 0; i < points.size(); i++) {
        points[i] = center1 + points[i];
    }

    return points;
}

QTransform mapToRect(const QRectF &rect)
{
    return
        QTransform(rect.width(), 0, 0, rect.height(),
                   rect.x(), rect.y());
}

QTransform mapToRectInverse(const QRectF &rect)
{
    return
        QTransform::fromTranslate(-rect.x(), -rect.y()) *
        QTransform::fromScale(rect.width() != 0 ? 1.0 / rect.width() : 0.0,
                              rect.height() != 0 ? 1.0 / rect.height() : 0.0);
}

bool fuzzyMatrixCompare(const QTransform &t1, const QTransform &t2, qreal delta) {
    return
            qAbs(t1.m11() - t2.m11()) < delta &&
            qAbs(t1.m12() - t2.m12()) < delta &&
            qAbs(t1.m13() - t2.m13()) < delta &&
            qAbs(t1.m21() - t2.m21()) < delta &&
            qAbs(t1.m22() - t2.m22()) < delta &&
            qAbs(t1.m23() - t2.m23()) < delta &&
            qAbs(t1.m31() - t2.m31()) < delta &&
            qAbs(t1.m32() - t2.m32()) < delta &&
            qAbs(t1.m33() - t2.m33()) < delta;
}

bool fuzzyPointCompare(const QPointF &p1, const QPointF &p2)
{
    return qFuzzyCompare(p1.x(), p2.x()) && qFuzzyCompare(p1.y(), p2.y());
}


bool fuzzyPointCompare(const QPointF &p1, const QPointF &p2, qreal delta)
{
    return qAbs(p1.x() - p2.x()) < delta && qAbs(p1.y() - p2.y()) < delta;
}


/********************************************************/
/*             DecomposedMatix                          */
/********************************************************/

DecomposedMatix::DecomposedMatix()
{
}

DecomposedMatix::DecomposedMatix(const QTransform &t0)
{
    QTransform t(t0);

    QTransform projMatrix;

    if (t.m33() == 0.0 || t0.determinant() == 0.0) {
        qWarning() << "Cannot decompose matrix!" << t;
        valid = false;
        return;
    }

    if (t.type() == QTransform::TxProject) {
        QTransform affineTransform(t.toAffine());
        projMatrix = affineTransform.inverted() * t;

        t = affineTransform;
        proj[0] = projMatrix.m13();
        proj[1] = projMatrix.m23();
        proj[2] = projMatrix.m33();
    }


    std::array<QVector3D, 3> rows;

    rows[0] = QVector3D(t.m11(), t.m12(), t.m13());
    rows[1] = QVector3D(t.m21(), t.m22(), t.m23());
    rows[2] = QVector3D(t.m31(), t.m32(), t.m33());

    if (!qFuzzyCompare(t.m33(), 1.0)) {
        const qreal invM33 = 1.0 / t.m33();

        for (auto &row : rows) {
            row *= invM33;
        }
    }

    dx = rows[2].x();
    dy = rows[2].y();

    rows[2] = QVector3D(0,0,1);

    scaleX = rows[0].length();
    rows[0] *= 1.0 / scaleX;

    shearXY = QVector3D::dotProduct(rows[0], rows[1]);
    rows[1] = rows[1] - shearXY * rows[0];

    scaleY = rows[1].length();
    rows[1] *= 1.0 / scaleY;
    shearXY *= 1.0 / scaleY;

    // If determinant is negative, one axis was flipped.
    qreal determinant = rows[0].x() * rows[1].y() - rows[0].y() * rows[1].x();
    if (determinant < 0) {
        // Flip axis with minimum unit vector dot product.
        if (rows[0].x() < rows[1].y()) {
            scaleX = -scaleX;
            rows[0] = -rows[0];
        } else {
            scaleY = -scaleY;
            rows[1] = -rows[1];
        }
        shearXY = - shearXY;
    }

    angle = kisRadiansToDegrees(std::atan2(rows[0].y(), rows[0].x()));

    if (angle != 0.0) {
        // Rotate(-angle) = [cos(angle), sin(angle), -sin(angle), cos(angle)]
        //                = [row0x, -row0y, row0y, row0x]
        // Thanks to the normalization above.

        qreal sn = -rows[0].y();
        qreal cs = rows[0].x();
        qreal m11 = rows[0].x();
        qreal m12 = rows[0].y();
        qreal m21 = rows[1].x();
        qreal m22 = rows[1].y();
        rows[0].setX(cs * m11 + sn * m21);
        rows[0].setY(cs * m12 + sn * m22);
        rows[1].setX(-sn * m11 + cs * m21);
        rows[1].setY(-sn * m12 + cs * m22);
    }

    QTransform leftOver(
                rows[0].x(), rows[0].y(), rows[0].z(),
            rows[1].x(), rows[1].y(), rows[1].z(),
            rows[2].x(), rows[2].y(), rows[2].z());

    KIS_SAFE_ASSERT_RECOVER_NOOP(fuzzyMatrixCompare(leftOver, QTransform(), 1e-4));
}

inline QTransform toQTransformStraight(const Eigen::Matrix3d &m)
{
    return QTransform(m(0,0), m(0,1), m(0,2),
                      m(1,0), m(1,1), m(1,2),
                      m(2,0), m(2,1), m(2,2));
}

inline Eigen::Matrix3d fromQTransformStraight(const QTransform &t)
{
    Eigen::Matrix3d m;

    m << t.m11() , t.m12() , t.m13()
        ,t.m21() , t.m22() , t.m23()
        ,t.m31() , t.m32() , t.m33();

    return m;
}

std::pair<QPointF, QTransform> transformEllipse(const QPointF &axes, const QTransform &fullLocalToGlobal)
{
    KisAlgebra2D::DecomposedMatix decomposed(fullLocalToGlobal);
    const QTransform localToGlobal =
            decomposed.scaleTransform() *
            decomposed.shearTransform() *
            decomposed.rotateTransform();

    const QTransform localEllipse = QTransform(1.0 / pow2(axes.x()), 0.0, 0.0,
                                               0.0, 1.0 / pow2(axes.y()), 0.0,
                                               0.0, 0.0, 1.0);


    const QTransform globalToLocal = localToGlobal.inverted();

    Eigen::Matrix3d eqM =
        fromQTransformStraight(globalToLocal *
                               localEllipse *
                               globalToLocal.transposed());

//    std::cout << "eqM:" << std::endl << eqM << std::endl;

    Eigen::EigenSolver<Eigen::Matrix3d> eigenSolver(eqM);

    const Eigen::Matrix3d T = eigenSolver.eigenvalues().real().asDiagonal();
    const Eigen::Matrix3d U = eigenSolver.eigenvectors().real();

    const Eigen::Matrix3d Ti = eigenSolver.eigenvalues().imag().asDiagonal();
    const Eigen::Matrix3d Ui = eigenSolver.eigenvectors().imag();

    KIS_SAFE_ASSERT_RECOVER_NOOP(Ti.isZero());
    KIS_SAFE_ASSERT_RECOVER_NOOP(Ui.isZero());
    KIS_SAFE_ASSERT_RECOVER_NOOP((U * U.transpose()).isIdentity());

//    std::cout << "T:" << std::endl << T << std::endl;
//    std::cout << "U:" << std::endl << U << std::endl;

//    std::cout << "Ti:" << std::endl << Ti << std::endl;
//    std::cout << "Ui:" << std::endl << Ui << std::endl;

//    std::cout << "UTU':" << std::endl << U * T * U.transpose() << std::endl;

    const qreal newA = 1.0 / std::sqrt(T(0,0) * T(2,2));
    const qreal newB = 1.0 / std::sqrt(T(1,1) * T(2,2));

    const QTransform newGlobalToLocal = toQTransformStraight(U);
    const QTransform newLocalToGlobal = QTransform::fromScale(-1,-1) *
            newGlobalToLocal.inverted() *
            decomposed.translateTransform();

    return std::make_pair(QPointF(newA, newB), newLocalToGlobal);
}

QPointF alignForZoom(const QPointF &pt, qreal zoom)
{
    return QPointF((pt * zoom).toPoint()) / zoom;
}

boost::optional<QPointF> intersectLines(const QLineF &boundedLine, const QLineF &unboundedLine)
{
    const QPointF B1 = unboundedLine.p1();
    const QPointF A1 = unboundedLine.p2() - unboundedLine.p1();

    const QPointF B2 = boundedLine.p1();
    const QPointF A2 = boundedLine.p2() - boundedLine.p1();

    Eigen::Matrix<float, 2, 2> A;
    A << A1.x(), -A2.x(),
         A1.y(), -A2.y();

    Eigen::Matrix<float, 2, 1> B;
    B << B2.x() - B1.x(),
         B2.y() - B1.y();

    if (qFuzzyIsNull(A.determinant())) {
        return boost::none;
    }

    Eigen::Matrix<float, 2, 1> T;

    T = A.inverse() * B;

    const qreal t2 = T(1,0);

    if (t2 < 0 || t2 > 1.0) {
        return boost::none;
    }

    return t2 * A2 + B2;
}

boost::optional<QPointF> intersectLines(const QPointF &p1, const QPointF &p2,
                                        const QPointF &q1, const QPointF &q2)
{
    return intersectLines(QLineF(p1, p2), QLineF(q1, q2));
}

QVector<QPointF> findTrianglePoint(const QPointF &p1, const QPointF &p2, qreal a, qreal b)
{
    using KisAlgebra2D::norm;
    using KisAlgebra2D::dotProduct;

    QVector<QPointF> result;

    const QPointF p = p2 - p1;

    const qreal pSq = dotProduct(p, p);

    const qreal T =  0.5 * (-pow2(b) + pow2(a) + pSq);

    if (p.isNull()) return result;

    if (qAbs(p.x()) > qAbs(p.y())) {
        const qreal A = 1.0;
        const qreal B2 = -T * p.y() / pSq;
        const qreal C = pow2(T) / pSq - pow2(a * p.x()) / pSq;

        const qreal D4 = pow2(B2) - A * C;

        if (D4 > 0 || qFuzzyIsNull(D4)) {
            if (qFuzzyIsNull(D4)) {
                const qreal y = -B2 / A;
                const qreal x = (T - y * p.y()) / p.x();
                result << p1 + QPointF(x, y);
            } else {
                const qreal y1 = (-B2 + std::sqrt(D4)) / A;
                const qreal x1 = (T - y1 * p.y()) / p.x();
                result << p1 + QPointF(x1, y1);

                const qreal y2 = (-B2 - std::sqrt(D4)) / A;
                const qreal x2 = (T - y2 * p.y()) / p.x();
                result << p1 + QPointF(x2, y2);
            }
        }
    } else {
        const qreal A = 1.0;
        const qreal B2 = -T * p.x() / pSq;
        const qreal C = pow2(T) / pSq - pow2(a * p.y()) / pSq;

        const qreal D4 = pow2(B2) - A * C;

        if (D4 > 0 || qFuzzyIsNull(D4)) {
            if (qFuzzyIsNull(D4)) {
                const qreal x = -B2 / A;
                const qreal y = (T - x * p.x()) / p.y();
                result << p1 + QPointF(x, y);
            } else {
                const qreal x1 = (-B2 + std::sqrt(D4)) / A;
                const qreal y1 = (T - x1 * p.x()) / p.y();
                result << p1 + QPointF(x1, y1);

                const qreal x2 = (-B2 - std::sqrt(D4)) / A;
                const qreal y2 = (T - x2 * p.x()) / p.y();
                result << p1 + QPointF(x2, y2);
            }
        }
    }

    return result;
}

boost::optional<QPointF> findTrianglePointNearest(const QPointF &p1, const QPointF &p2, qreal a, qreal b, const QPointF &nearest)
{
    const QVector<QPointF> points = findTrianglePoint(p1, p2, a, b);

    boost::optional<QPointF> result;

    if (points.size() == 1) {
        result = points.first();
    } else if (points.size() > 1) {
        result = kisDistance(points.first(), nearest) < kisDistance(points.last(), nearest) ? points.first() : points.last();
    }

    return result;
}

QPointF moveElasticPoint(const QPointF &pt, const QPointF &base, const QPointF &newBase, const QPointF &wingA, const QPointF &wingB)
{
    using KisAlgebra2D::norm;
    using KisAlgebra2D::dotProduct;
    using KisAlgebra2D::crossProduct;

    const QPointF vecL = base - pt;
    const QPointF vecLa = wingA - pt;
    const QPointF vecLb = wingB - pt;

    const qreal L = norm(vecL);
    const qreal La = norm(vecLa);
    const qreal Lb = norm(vecLb);

    const qreal sinMuA = crossProduct(vecLa, vecL) / (La * L);
    const qreal sinMuB = crossProduct(vecL, vecLb) / (Lb * L);

    const qreal cosMuA = dotProduct(vecLa, vecL) / (La * L);
    const qreal cosMuB = dotProduct(vecLb, vecL) / (Lb * L);

    const qreal dL = dotProduct(newBase - base, -vecL) / L;


    const qreal divB = (cosMuB + La / Lb * sinMuB * cosMuA / sinMuA) / L +
            (cosMuB + sinMuB * cosMuA / sinMuA) / Lb;
    const qreal dLb = dL / ( L * divB);

    const qreal divA = (cosMuA + Lb / La * sinMuA * cosMuB / sinMuB) / L +
            (cosMuA + sinMuA * cosMuB / sinMuB) / La;
    const qreal dLa = dL / ( L * divA);

    boost::optional<QPointF> result =
        KisAlgebra2D::findTrianglePointNearest(wingA, wingB, La + dLa, Lb + dLb, pt);

    const QPointF resultPoint = result ? *result : pt;

    return resultPoint;
}

#ifdef HAVE_GSL

struct ElasticMotionData
{
    QPointF oldBasePos;
    QPointF newBasePos;
    QVector<QPointF> anchorPoints;

    QPointF oldResultPoint;
};

double elasticMotionError(const gsl_vector * x, void *paramsPtr)
{
    using KisAlgebra2D::norm;
    using KisAlgebra2D::dotProduct;
    using KisAlgebra2D::crossProduct;

    const QPointF newResultPoint(gsl_vector_get(x, 0), gsl_vector_get(x, 1));

    const ElasticMotionData *p = static_cast<const ElasticMotionData*>(paramsPtr);

    const QPointF vecL = newResultPoint - p->newBasePos;
    const qreal L = norm(vecL);

    const qreal deltaL = L - kisDistance(p->oldBasePos, p->oldResultPoint);

    QVector<qreal> deltaLi;
    QVector<qreal> Li;
    QVector<qreal> cosMuI;
    QVector<qreal> sinMuI;
    Q_FOREACH (const QPointF &anchorPoint, p->anchorPoints) {
        const QPointF vecLi = newResultPoint - anchorPoint;
        const qreal _Li = norm(vecLi);

        Li << _Li;
        deltaLi << _Li - kisDistance(p->oldResultPoint, anchorPoint);
        cosMuI << dotProduct(vecLi, vecL) / (_Li * L);
        sinMuI << crossProduct(vecL, vecLi) / (_Li * L);
    }

    qreal finalError = 0;

    qreal tangentialForceSum = 0;
    for (int i = 0; i < p->anchorPoints.size(); i++) {
        const qreal sum = deltaLi[i] * sinMuI[i] / Li[i];
        tangentialForceSum += sum;
    }

    finalError += pow2(tangentialForceSum);

    qreal normalForceSum = 0;
    {
        qreal sum = 0;
        for (int i = 0; i < p->anchorPoints.size(); i++) {
            sum += deltaLi[i] * cosMuI[i] / Li[i];
        }
        normalForceSum = (-deltaL) / L - sum;
    }

    finalError += pow2(normalForceSum);

    return finalError;
}


#endif /* HAVE_GSL */

QPointF moveElasticPoint(const QPointF &pt,
                         const QPointF &base, const QPointF &newBase,
                         const QVector<QPointF> &anchorPoints)
{
    const QPointF offset = newBase - base;

    QPointF newResultPoint = pt + offset;

#ifdef HAVE_GSL

    ElasticMotionData data;
    data.newBasePos = newBase;
    data.oldBasePos = base;
    data.anchorPoints = anchorPoints;
    data.oldResultPoint = pt;

    const gsl_multimin_fminimizer_type *T =
        gsl_multimin_fminimizer_nmsimplex2;
    gsl_multimin_fminimizer *s = 0;
    gsl_vector *ss, *x;
    gsl_multimin_function minex_func;

    size_t iter = 0;
    int status;
    double size;

    /* Starting point */
    x = gsl_vector_alloc (2);
    gsl_vector_set (x, 0, newResultPoint.x());
    gsl_vector_set (x, 1, newResultPoint.y());

    /* Set initial step sizes to 0.1 */
    ss = gsl_vector_alloc (2);
    gsl_vector_set (ss, 0, 0.1 * offset.x());
    gsl_vector_set (ss, 1, 0.1 * offset.y());

    /* Initialize method and iterate */
    minex_func.n = 2;
    minex_func.f = elasticMotionError;
    minex_func.params = (void*)&data;

    s = gsl_multimin_fminimizer_alloc (T, 2);
    gsl_multimin_fminimizer_set (s, &minex_func, x, ss);

    do
    {
        iter++;
        status = gsl_multimin_fminimizer_iterate(s);

        if (status)
            break;

        size = gsl_multimin_fminimizer_size (s);
        status = gsl_multimin_test_size (size, 1e-6);

        /**
         * Sometimes the algorithm may converge to a wrond point,
         * then just try to force it search better or return invalid
         * result.
         */
        if (status == GSL_SUCCESS && elasticMotionError(s->x, &data) > 0.5) {
            status = GSL_CONTINUE;
        }

        if (status == GSL_SUCCESS)
        {
//             qDebug() << "*******Converged to minimum";
//             qDebug() << gsl_vector_get (s->x, 0)
//                      << gsl_vector_get (s->x, 1)
//                      << "|" << s->fval << size;
//             qDebug() << ppVar(iter);

             newResultPoint.rx() = gsl_vector_get (s->x, 0);
             newResultPoint.ry() = gsl_vector_get (s->x, 1);
        }
    }
    while (status == GSL_CONTINUE && iter < 10000);

    if (status != GSL_SUCCESS) {
        ENTER_FUNCTION() << "failed to find point" << ppVar(pt) << ppVar(base) << ppVar(newBase);
    }

    gsl_vector_free(x);
    gsl_vector_free(ss);
    gsl_multimin_fminimizer_free (s);
#endif

    return newResultPoint;
}
}
