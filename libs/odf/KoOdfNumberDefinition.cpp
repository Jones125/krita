/* This file is part of the KDE project

   Copyright (C) 2010 Boudewijn Rempt
   Copyright (C) 2011 Mojtaba Shahi Senobari <mojtaba.shahi3000@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */
#include "KoOdfNumberDefinition.h"

#include "KoXmlNS.h"
#include "KoXmlWriter.h"
#include "KoXmlReader.h"

class Q_DECL_HIDDEN KoOdfNumberDefinition::Private
{
public:
    QString prefix;
    QString suffix;
    KoOdfNumberDefinition::FormatSpecification formatSpecification;
    bool letterSynchronization;
};

KoOdfNumberDefinition::KoOdfNumberDefinition()
    : d(new Private())
{
    d->formatSpecification = Numeric;
    d->letterSynchronization = false;
}

KoOdfNumberDefinition::KoOdfNumberDefinition(const KoOdfNumberDefinition &other)
    : d(new Private())
{
    d->prefix = other.d->prefix;
    d->suffix = other.d->suffix;
    d->formatSpecification = other.d->formatSpecification;
    d->letterSynchronization = other.d->letterSynchronization;
}

KoOdfNumberDefinition &KoOdfNumberDefinition::operator=(const KoOdfNumberDefinition &other)
{
    d->prefix = other.d->prefix;
    d->suffix = other.d->suffix;
    d->formatSpecification = other.d->formatSpecification;
    d->letterSynchronization = other.d->letterSynchronization;

    return *this;
}

KoOdfNumberDefinition::~KoOdfNumberDefinition()
{
    delete d;
}

void KoOdfNumberDefinition::loadOdf(const KoXmlElement &element)
{
    const QString format = element.attributeNS(KoXmlNS::style, "num-format", QString());
    if (format.isEmpty()) {
        //do nothing fall back to what we had.
    }
    else if (format[0] == '1') {
        d->formatSpecification = Numeric;
    }
    else if (format[0] == 'a') {
        d->formatSpecification = AlphabeticLowerCase;
    }
    else if (format[0] == 'A') {
        d->formatSpecification = AlphabeticUpperCase;
    }
    else if (format[0] == 'i') {
        d->formatSpecification = RomanLowerCase;
    }
    else if (format[0] == 'I') {
        d->formatSpecification = RomanUpperCase;
    }
    else if (format == QString::fromUtf8("??, ??, ??, ...")){
        d->formatSpecification = ArabicAlphabet;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")){
        d->formatSpecification = Thai;
    }
    else if (format == QString::fromUtf8("??, ??, ??, ...")) {
        d->formatSpecification = Abjad;
    }
    else if (format == QString::fromUtf8("???,???, ???, ... ")) {
        d->formatSpecification = AbjadMinor;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")) {
        d->formatSpecification = Telugu;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")) {
        d->formatSpecification = Tamil;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")) {
        d->formatSpecification = Oriya;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")) {
        d->formatSpecification = Malayalam;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")) {
        d->formatSpecification = Kannada;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")) {
        d->formatSpecification = Gurumukhi;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")) {
        d->formatSpecification = Gujarati;
    }
    else if (format == QString::fromUtf8("???, ???, ???, ...")) {
        d->formatSpecification = Bengali;
    }
    else {
        d->formatSpecification = Numeric;
    }

    //The style:num-prefix and style:num-suffix attributes specify what to display before and after the number.
    d->prefix = element.attributeNS(KoXmlNS::style, "num-prefix", d->prefix);
    d->suffix = element.attributeNS(KoXmlNS::style, "num-suffix", d->suffix);

    d->letterSynchronization = (element.attributeNS(KoXmlNS::style, "num-letter-sync", d->letterSynchronization ? "true" : "false") == "true");
}

void KoOdfNumberDefinition::saveOdf(KoXmlWriter *writer) const
{
    if (!d->prefix.isNull()) {
        writer->addAttribute("style:num-prefix", d->prefix);
    }

    if (!d->suffix.isNull()) {
        writer->addAttribute("style:num-suffix", d->suffix);
    }
    QByteArray format;
    switch(d->formatSpecification) {
    case Numeric:
        format = "1";
        break;
    case AlphabeticLowerCase:
        format = "a";
        break;
    case AlphabeticUpperCase:
        format = "A";
        break;
    case RomanLowerCase:
        format = "i";
        break;
    case RomanUpperCase:
        format = "I";
        break;
    case ArabicAlphabet:
        format = "??, ??, ??, ...";
        break;
    case Thai:
        format = "???, ???, ???, ...";
        break;
    case Telugu:
        format = "???, ???, ???, ...";
        break;
    case Tamil:
        format = "???, ???, ???, ...";
        break;
    case Oriya:
        format = "???, ???, ???, ...";
        break;
    case Malayalam:
        format = "???, ???, ???, ...";
        break;
    case Kannada:
        format = "???, ???, ???, ...";
        break;
    case Gurumukhi:
        format = "???, ???, ???, ...";
        break;
    case Gujarati:
        format = "???, ???, ???, ...";
        break;
    case Bengali:
        format = "???, ???, ???, ...";
        break;
    case Empty:
    default:
        ;
    };
    if (!format.isNull()) {
        writer->addAttribute("style:num-format", format);
    }

    if (d->letterSynchronization) {
        writer->addAttribute("style:num-letter-sync", "true");
    }
}

QString KoOdfNumberDefinition::formattedNumber(int number, KoOdfNumberDefinition *defaultDefinition) const
{
   switch(d->formatSpecification) {
    case Numeric:
        return QString::number(number);
        break;

    case AlphabeticLowerCase:
    {
        if (d->letterSynchronization) {
            int loop = (number-1)/26;
            int rem = (number-1)%26;
            QChar letter = (char)(rem+97);
            QString alpha;
            for (int i=0; i<=loop; i++) {
                alpha.append(letter);
            }
            return alpha;
        } else {
            int loop = (number-1)/26;
            QChar letter;
            QString alpha;
            if (loop>0) {
                letter = (char)(loop+96);
                alpha.append(letter);
            }
            int rem = (number -1)%26;
            letter = (char)(rem+97);
            alpha.append(letter);
            return alpha;
        }
        break;
    }
    case AlphabeticUpperCase:
    {
        if (d->letterSynchronization) {
            int loop = (number-1)/26;
            int rem = (number-1)%26;
            QChar letter = (char)(rem+65);
            QString alpha;
            for (int i=0; i<=loop; i++) {
                alpha.append(letter);
            }
            return alpha;
        } else {
            int loop = (number-1)/26;
            QChar letter;
            QString alpha;
            if (loop>0) {
                letter = (char)(loop+64);
                alpha.append(letter);
            }
            int rem = (number -1)%26;
            letter = (char)(rem+65);
            alpha.append(letter);
            return alpha;
        }
        break;
    }
    case RomanLowerCase:
    {
        QString roman;
        int loop = number/1000;
        for (int i=1; i<=loop && number/1000!=0; i++) {
             roman.append("m");
        }
        number = number%1000;
        loop = number/500;
        if (loop > 0) {
            roman.append("d");
        }
        number = number%500;
        loop = number/100;
        for (int i=1; i<=loop && number/100!=0; i++) {
            roman.append("c");
        }
        number = number%100;
        loop = number/50;
        if (loop > 0) {
            roman.append("l");
        }
        number = number%50;
        loop = number/10;
        for (int i=1; i<=loop && number/10!=0; i++) {
             roman.append("x");
        }
        number = number%10;
        if (number>=5 && number<=8) {
             loop = number%5;
             roman.append("v");
             for (int i=1;i<=loop;i++)
                roman.append("i");
        }
        else if (number==9) {
             roman.append("ix");
        }
        else if (number>=1 && number<=3) {
             for (int i=1; i<=number; i++)
                roman.append("i");
        }
        else if (number==4)
            roman.append("iv");

        return roman;
        break;
    }
    case RomanUpperCase:
    {
        QString roman;
        int loop = number/1000;
        for (int i=1; i<=loop && number/1000!=0; i++) {
             roman.append("M");
        }
        number = number%1000;
        loop = number/500;
        if (loop > 0) {
            roman.append("D");
        }
        number = number%500;
        loop = number/100;
        for (int i=1; i<=loop && number/100!=0; i++) {
            roman.append("C");
        }
        number = number%100;
        loop = number/50;
        if (loop > 0) {
             roman.append("L");
        }
        number = number%50;
        loop = number/10;
        for (int i=1; i<=loop && number/10!=0; i++) {
             roman.append("X");
        }
        number = number%10;
        if (number>=5 && number<=8) {
             loop = number%5;
             roman.append("V");
             for (int i=1; i<=loop; i++)
                roman.append("I");
        }
        else if (number==9) {
             roman.append("IX");
        }
        else if (number>=1 && number<=3) {
             for (int i=1; i<=number; i++)
                roman.append("I");
        }
        else if (number==4)
            roman.append("IV");

        return roman;
    }
    case Empty:
        if (defaultDefinition) {
            return defaultDefinition->formattedNumber(number);
        }

        break;
    default:
        ;
    };

    return "";
}


QString KoOdfNumberDefinition::prefix() const
{
    return d->prefix;
}

void KoOdfNumberDefinition::setPrefix(const QString &prefix)
{
    d->prefix = prefix;
}

QString KoOdfNumberDefinition::suffix() const
{
    return d->suffix;
}

void KoOdfNumberDefinition::setSuffix(const QString &suffix)
{
    d->suffix = suffix;
}

KoOdfNumberDefinition::FormatSpecification KoOdfNumberDefinition::formatSpecification() const
{
    return d->formatSpecification;
}

void KoOdfNumberDefinition::setFormatSpecification(FormatSpecification formatSpecification)
{
    d->formatSpecification = formatSpecification;
}

bool KoOdfNumberDefinition::letterSynchronization() const
{
    return d->letterSynchronization;
}

void KoOdfNumberDefinition::setLetterSynchronization(bool letterSynchronization)
{
    d->letterSynchronization = letterSynchronization;
}
