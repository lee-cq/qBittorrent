/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2019, 2021  Prince Gupta <jagannatharjun11@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If
 * you modify file(s), you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 */

#include "uithememanager.h"

#include <QPalette>
#include <QPixmapCache>
#include <QResource>

#include "base/global.h"
#include "base/logger.h"
#include "base/path.h"
#include "base/preferences.h"
#include "uithemecommon.h"

namespace
{
    bool isDarkTheme()
    {
        const QPalette palette = qApp->palette();
        const QColor &color = palette.color(QPalette::Active, QPalette::Base);
        return (color.lightness() < 127);
    }
}

UIThemeManager *UIThemeManager::m_instance = nullptr;

void UIThemeManager::freeInstance()
{
    delete m_instance;
    m_instance = nullptr;
}

void UIThemeManager::initInstance()
{
    if (!m_instance)
        m_instance = new UIThemeManager;
}

UIThemeManager::UIThemeManager()
    : m_useCustomTheme {Preferences::instance()->useCustomUITheme()}
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    , m_useSystemIcons {Preferences::instance()->useSystemIcons()}
#endif
{
    if (m_useCustomTheme)
    {
        const Path themePath = Preferences::instance()->customUIThemePath();

        if (themePath.hasExtension(u".qbtheme"_qs))
        {
            if (QResource::registerResource(themePath.data(), u"/uitheme"_qs))
                m_themeSource = std::make_unique<QRCThemeSource>();
            else
                LogMsg(tr("Failed to load UI theme from file: \"%1\"").arg(themePath.toString()), Log::WARNING);
        }
        else if (themePath.filename() == CONFIG_FILE_NAME)
        {
            m_themeSource = std::make_unique<FolderThemeSource>(themePath.parentPath());
        }
    }

    if (!m_themeSource)
        m_themeSource = std::make_unique<DefaultThemeSource>();

    if (m_useCustomTheme)
    {
        applyPalette();
        applyStyleSheet();
    }
}

UIThemeManager *UIThemeManager::instance()
{
    return m_instance;
}

void UIThemeManager::applyStyleSheet() const
{
    qApp->setStyleSheet(QString::fromUtf8(m_themeSource->readStyleSheet()));
}

QIcon UIThemeManager::getIcon(const QString &iconId, [[maybe_unused]] const QString &fallback) const
{
    const auto colorMode = isDarkTheme() ? ColorMode::Dark : ColorMode::Light;
    auto &icons = (colorMode == ColorMode::Dark) ? m_darkModeIcons : m_icons;

    const auto iter = icons.find(iconId);
    if (iter != icons.end())
        return *iter;

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    // Don't cache system icons because users might change them at run time
    if (m_useSystemIcons)
    {
        auto icon = QIcon::fromTheme(iconId);
        if (icon.isNull() || icon.availableSizes().isEmpty())
            icon = QIcon::fromTheme(fallback, QIcon(m_themeSource->getIconPath(iconId, colorMode).data()));
        return icon;
    }
#endif

    const QIcon icon {m_themeSource->getIconPath(iconId, colorMode).data()};
    icons[iconId] = icon;
    return icon;
}

QIcon UIThemeManager::getFlagIcon(const QString &countryIsoCode) const
{
    if (countryIsoCode.isEmpty())
        return {};

    const QString key = countryIsoCode.toLower();
    const auto iter = m_flags.find(key);
    if (iter != m_flags.end())
        return *iter;

    const QIcon icon {u":/icons/flags/" + key + u".svg"};
    m_flags[key] = icon;
    return icon;
}

QPixmap UIThemeManager::getScaledPixmap(const QString &iconId, const int height) const
{
    // (workaround) svg images require the use of `QIcon()` to load and scale losslessly,
    // otherwise other image classes will convert it to pixmap first and follow-up scaling will become lossy.

    Q_ASSERT(height > 0);

    const QString cacheKey = iconId + u'@' + QString::number(height);

    QPixmap pixmap;
    if (!QPixmapCache::find(cacheKey, &pixmap))
    {
        pixmap = getIcon(iconId).pixmap(height);
        QPixmapCache::insert(cacheKey, pixmap);
    }

    return pixmap;
}

QColor UIThemeManager::getColor(const QString &id) const
{
    const QColor color = m_themeSource->getColor(id, (isDarkTheme() ? ColorMode::Dark : ColorMode::Light));
    Q_ASSERT(color.isValid());

    return color;
}

void UIThemeManager::applyPalette() const
{
    struct ColorDescriptor
    {
        QString id;
        QPalette::ColorRole colorRole;
        QPalette::ColorGroup colorGroup;
    };

    const ColorDescriptor paletteColorDescriptors[] =
    {
        {u"Palette.Window"_qs, QPalette::Window, QPalette::Normal},
        {u"Palette.WindowText"_qs, QPalette::WindowText, QPalette::Normal},
        {u"Palette.Base"_qs, QPalette::Base, QPalette::Normal},
        {u"Palette.AlternateBase"_qs, QPalette::AlternateBase, QPalette::Normal},
        {u"Palette.Text"_qs, QPalette::Text, QPalette::Normal},
        {u"Palette.ToolTipBase"_qs, QPalette::ToolTipBase, QPalette::Normal},
        {u"Palette.ToolTipText"_qs, QPalette::ToolTipText, QPalette::Normal},
        {u"Palette.BrightText"_qs, QPalette::BrightText, QPalette::Normal},
        {u"Palette.Highlight"_qs, QPalette::Highlight, QPalette::Normal},
        {u"Palette.HighlightedText"_qs, QPalette::HighlightedText, QPalette::Normal},
        {u"Palette.Button"_qs, QPalette::Button, QPalette::Normal},
        {u"Palette.ButtonText"_qs, QPalette::ButtonText, QPalette::Normal},
        {u"Palette.Link"_qs, QPalette::Link, QPalette::Normal},
        {u"Palette.LinkVisited"_qs, QPalette::LinkVisited, QPalette::Normal},
        {u"Palette.Light"_qs, QPalette::Light, QPalette::Normal},
        {u"Palette.Midlight"_qs, QPalette::Midlight, QPalette::Normal},
        {u"Palette.Mid"_qs, QPalette::Mid, QPalette::Normal},
        {u"Palette.Dark"_qs, QPalette::Dark, QPalette::Normal},
        {u"Palette.Shadow"_qs, QPalette::Shadow, QPalette::Normal},
        {u"Palette.WindowTextDisabled"_qs, QPalette::WindowText, QPalette::Disabled},
        {u"Palette.TextDisabled"_qs, QPalette::Text, QPalette::Disabled},
        {u"Palette.ToolTipTextDisabled"_qs, QPalette::ToolTipText, QPalette::Disabled},
        {u"Palette.BrightTextDisabled"_qs, QPalette::BrightText, QPalette::Disabled},
        {u"Palette.HighlightedTextDisabled"_qs, QPalette::HighlightedText, QPalette::Disabled},
        {u"Palette.ButtonTextDisabled"_qs, QPalette::ButtonText, QPalette::Disabled}
    };

    QPalette palette = qApp->palette();
    for (const ColorDescriptor &colorDescriptor : paletteColorDescriptors)
    {
        // For backward compatibility, the palette color overrides are read from the section of the "light mode" colors
        const QColor newColor = m_themeSource->getColor(colorDescriptor.id, ColorMode::Light);
        if (newColor.isValid())
            palette.setColor(colorDescriptor.colorGroup, colorDescriptor.colorRole, newColor);
    }

    qApp->setPalette(palette);
}
