/*
 * themecomposer.cpp — Compose ColorPalette + FontPairing into a StyleManager
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "themecomposer.h"
#include "stylemanager.h"
#include "thememanager.h"
#include "paragraphstyle.h"
#include "characterstyle.h"
#include "tablestyle.h"

ThemeComposer::ThemeComposer(ThemeManager *themeManager, QObject *parent)
    : QObject(parent)
    , m_themeManager(themeManager)
{
}

void ThemeComposer::setColorPalette(const ColorPalette &palette)
{
    if (!(m_palette == palette)) {
        m_palette = palette;
        Q_EMIT compositionChanged();
    }
}

void ThemeComposer::setFontPairing(const FontPairing &pairing)
{
    if (!(m_pairing == pairing)) {
        m_pairing = pairing;
        Q_EMIT compositionChanged();
    }
}

void ThemeComposer::compose(StyleManager *target)
{
    if (!target || !m_themeManager)
        return;

    // 1. Load hardcoded defaults to establish the style hierarchy
    m_themeManager->loadDefaults(target);

    // 2. Apply font pairing (font families)
    applyFontPairing(target);

    // 3. Apply color palette (foreground/background colors)
    applyColorPalette(target);

    // 4. Ensure parent hierarchy is intact after all modifications
    m_themeManager->assignDefaultParents(target);
}

void ThemeComposer::applyFontPairing(StyleManager *target)
{
    // Body font → Default Paragraph Style + Default Character Style
    if (!m_pairing.body.family.isEmpty()) {
        ParagraphStyle *dps = target->paragraphStyle(QStringLiteral("Default Paragraph Style"));
        if (dps)
            dps->setFontFamily(m_pairing.body.family);

        CharacterStyle *dcs = target->characterStyle(QStringLiteral("Default Character Style"));
        if (dcs)
            dcs->setFontFamily(m_pairing.body.family);
    }

    // Heading font → Heading paragraph style
    if (!m_pairing.heading.family.isEmpty()) {
        ParagraphStyle *heading = target->paragraphStyle(QStringLiteral("Heading"));
        if (heading)
            heading->setFontFamily(m_pairing.heading.family);
    }

    // Mono font → Code character style
    if (!m_pairing.mono.family.isEmpty()) {
        CharacterStyle *code = target->characterStyle(QStringLiteral("Code"));
        if (code)
            code->setFontFamily(m_pairing.mono.family);
    }
}

void ThemeComposer::applyColorPalette(StyleManager *target)
{
    // text → Default Paragraph Style.foreground, Default Character Style.foreground
    {
        QColor c = m_palette.text();
        if (c.isValid()) {
            ParagraphStyle *dps = target->paragraphStyle(QStringLiteral("Default Paragraph Style"));
            if (dps)
                dps->setForeground(c);

            CharacterStyle *dcs = target->characterStyle(QStringLiteral("Default Character Style"));
            if (dcs)
                dcs->setForeground(c);
        }
    }

    // headingText → Heading.foreground (inherited by Heading1-6)
    {
        QColor c = m_palette.headingText();
        if (c.isValid()) {
            ParagraphStyle *heading = target->paragraphStyle(QStringLiteral("Heading"));
            if (heading)
                heading->setForeground(c);
        }
    }

    // blockquoteText → BlockQuote.foreground
    {
        QColor c = m_palette.blockquoteText();
        if (c.isValid()) {
            ParagraphStyle *bq = target->paragraphStyle(QStringLiteral("BlockQuote"));
            if (bq)
                bq->setForeground(c);
        }
    }

    // linkText → Link.foreground (character style)
    {
        QColor c = m_palette.linkText();
        if (c.isValid()) {
            CharacterStyle *link = target->characterStyle(QStringLiteral("Link"));
            if (link)
                link->setForeground(c);
        }
    }

    // codeText → InlineCode.foreground (character style)
    {
        QColor c = m_palette.codeText();
        if (c.isValid()) {
            CharacterStyle *ic = target->characterStyle(QStringLiteral("InlineCode"));
            if (ic)
                ic->setForeground(c);
        }
    }

    // surfaceCode → CodeBlock.background (paragraph style)
    {
        QColor c = m_palette.surfaceCode();
        if (c.isValid()) {
            ParagraphStyle *cb = target->paragraphStyle(QStringLiteral("CodeBlock"));
            if (cb)
                cb->setBackground(c);
        }
    }

    // surfaceInlineCode → InlineCode.background (character style)
    {
        QColor c = m_palette.surfaceInlineCode();
        if (c.isValid()) {
            CharacterStyle *ic = target->characterStyle(QStringLiteral("InlineCode"));
            if (ic)
                ic->setBackground(c);
        }
    }

    // Table style colors
    {
        TableStyle *ts = target->tableStyle(QStringLiteral("Default"));

        // If no Default table style exists yet, create one
        if (!ts) {
            TableStyle newTs(QStringLiteral("Default"));
            target->addTableStyle(newTs);
            ts = target->tableStyle(QStringLiteral("Default"));
        }

        if (ts) {
            // surfaceTableHeader → TableStyle.headerBackground
            QColor hdrBg = m_palette.surfaceTableHeader();
            if (hdrBg.isValid())
                ts->setHeaderBackground(hdrBg);

            // surfaceTableAlt → TableStyle.alternateRowColor
            QColor altRow = m_palette.surfaceTableAlt();
            if (altRow.isValid())
                ts->setAlternateRowColor(altRow);

            // borderOuter → TableStyle.outerBorder.color
            QColor borderOuter = m_palette.borderOuter();
            if (borderOuter.isValid()) {
                TableStyle::Border ob = ts->outerBorder();
                ob.color = borderOuter;
                ts->setOuterBorder(ob);
            }

            // borderInner → TableStyle.innerBorder.color
            QColor borderInner = m_palette.borderInner();
            if (borderInner.isValid()) {
                TableStyle::Border ib = ts->innerBorder();
                ib.color = borderInner;
                ts->setInnerBorder(ib);
            }

            // borderHeaderBottom → TableStyle.headerBottomBorder.color
            QColor borderHdrBottom = m_palette.borderHeaderBottom();
            if (borderHdrBottom.isValid()) {
                TableStyle::Border hbb = ts->headerBottomBorder();
                hbb.color = borderHdrBottom;
                ts->setHeaderBottomBorder(hbb);
            }
        }
    }
}
