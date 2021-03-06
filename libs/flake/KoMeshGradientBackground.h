#ifndef KOMESHGRADIENTBACKGROUND_H
#define KOMESHGRADIENTBACKGROUND_H

#include "KoShapeBackground.h"
#include <QSharedDataPointer>
#include "SvgMeshGradient.h"

class KRITAFLAKE_EXPORT KoMeshGradientBackground : public KoShapeBackground
{
public:
    KoMeshGradientBackground(const SvgMeshGradient *gradient, const QTransform &matrix = QTransform());
    ~KoMeshGradientBackground();

    void paint(QPainter &painter, KoShapePaintingContext &context, const QPainterPath &fillPath) const override;

    bool compareTo(const KoShapeBackground *other) const override;

    void fillStyle(KoGenStyle &style, KoShapeSavingContext &context) override;

    bool loadStyle(KoOdfLoadingContext &context, const QSizeF &shapeSize) override;

    SvgMeshGradient* gradient();
    QTransform transform();

private:
    class Private;
    QSharedDataPointer<Private> d;
};


#endif // KOMESHGRADIENTBACKGROUND_H
