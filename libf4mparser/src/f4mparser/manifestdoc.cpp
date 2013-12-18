/****************************************************************************
 * This file is part of libf4mparser.
 *
 * Copyright (C) 2013 rafirafi <rafirafi.at@gmail.com>
 *
 * Author(s):
 * rafirafi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 ***************************************************************************/

#include "manifestparser.h"

#include <cstring> // strcmp

// ns
const std::string ManifestDoc::m_nsF4mBase("http://ns.adobe.com/f4m/");

ManifestDoc::ManifestDoc(std::string url)
    : m_fileUrl(url), m_doc{NULL}, m_xpathCtx{NULL},
      m_major(0), m_minor(0), m_manifestLevel(UNKNOWN_LEVEL)
{
}

ManifestDoc::~ManifestDoc()
{
    if (m_doc) {
        xmlFreeDoc(m_doc);
        m_doc = NULL;
    }
    if (m_xpathCtx) {
        xmlXPathFreeContext(m_xpathCtx);
        m_xpathCtx = NULL;
    }
    xmlCleanupParser();
}

// the F4M 3.0 spec says only '1.0' '2.0' and '3.0' are legal
bool ManifestDoc::setVersion(std::string version)
{
    bool ret = true;
    int major = 1, minor = 0;
    size_t pos;
    if ((pos = version.find('.')) != std::string::npos) {
        try { major = std::stoi(version.substr(0, pos)); } catch (...) {
            major = 1;
            ret = false;
        }
        try { minor = std::stoi(version.substr(pos + 1)); } catch (...) {
            major = 1;
            minor = 0;
            ret = false;
        }
        m_major = major;
        m_minor = minor;
    } else {
        ret = false;
    }
    return ret;
}
