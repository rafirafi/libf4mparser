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
const char* const ManifestDoc::m_nsF4mVersion1 = "http://ns.adobe.com/f4m/1.0";
const char* const ManifestDoc::m_nsF4mVersion2 = "http://ns.adobe.com/f4m/2.0";

ManifestDoc::ManifestDoc(std::string url)
    : m_fileUrl(url), m_doc{NULL}, m_xpathCtx{NULL}, m_nsF4m{nullptr}, m_isMultiLevelStreamLevel(false)
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

int ManifestDoc::getF4mVersion() const
{
    if (!m_nsF4m) {
        return -1;
    }
    if (strcmp(m_nsF4m, m_nsF4mVersion2) == 0) {
        return 2;
    } else if (strcmp(m_nsF4m, m_nsF4mVersion1) == 0) {
        return 1;
    }
    return -1;
}
