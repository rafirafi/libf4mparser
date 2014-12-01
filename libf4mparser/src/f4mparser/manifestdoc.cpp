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

#include "manifestdoc.h"

#include <cstring> // strchr

#ifndef F4M_DEBUG
#define F4M_DLOG(x)
#else
#include <iostream>  // std::cerr
#define F4M_DLOG(x) do { x } while(0)
#endif

// ns
const std::string ManifestDoc::m_nsF4mBase("http://ns.adobe.com/f4m/");

ManifestDoc::ManifestDoc(std::string url)
    : m_fileUrl(url), m_major(0), m_minor(0), m_manifestLevel(UNKNOWN_LEVEL)
{
}

ManifestDoc::~ManifestDoc()
{
}

// the F4M 3.0 spec says only '1.0' '2.0' and '3.0' values are valid
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

std::string ManifestDoc::rootNs()
{
    return m_doc.child("manifest").attribute("xmlns").value();
}

std::string ManifestDoc::selectNs()
{
        return std::string("[namespace-uri()='" + rootNs() + "']");
}

bool ManifestDoc::setXmlDoc(std::vector<uint8_t> rawDoc)
{
    // set the raw buffer
    m_xmlRawBuffer = std::move(rawDoc);

    pugi::xml_parse_result result = m_doc.load_buffer_inplace(m_xmlRawBuffer.data(), m_xmlRawBuffer.size());
    if (!result) {
        F4M_DLOG(std::cerr << __func__ << " Error description: " << result.description() << "\n";);
        return false;
    }

    return true;
}
