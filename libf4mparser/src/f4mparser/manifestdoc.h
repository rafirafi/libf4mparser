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

#ifndef MANIFESTDOC_H
#define MANIFESTDOC_H

#include <string>
#include <vector>

#include <pugixml.hpp>

namespace pugi {
// stripped from an old version of pugixml
// temporary workaround till I figure something
const char_t* namespace_uri(const xml_node& node);
}

// manage what is specific to the manifest (not to the media presentation)

class ManifestDoc
{
public:
    static const std::string m_nsF4mBase;

    enum manifest_level_t {
        UNKNOWN_LEVEL,
        SLM_STREAM_LEVEL, // single-level manifest
        MLM_SET_LEVEL, // multi-level set-level manifest
        MLM_STREAM_LEVEL // multi-level stream-level manifest
    };

public:
    explicit ManifestDoc(std::string url);
    ~ManifestDoc();

    bool setVersion(std::string version);
    int  versionMajor() const { return m_major; }
    int  versionMinor() const { return m_minor; }

    manifest_level_t manifestLevel() const { return m_manifestLevel; }
    void setManifestLevel(manifest_level_t level) { m_manifestLevel = level; }

    bool isSetLevel() { return m_manifestLevel == MLM_SET_LEVEL; }
    bool isStreamLevel() { return m_manifestLevel == SLM_STREAM_LEVEL
                || m_manifestLevel == MLM_STREAM_LEVEL; }
    bool isSingleLevel() { return m_manifestLevel == SLM_STREAM_LEVEL; }
    bool isMultiLevelStreamLevel() { return m_manifestLevel == MLM_STREAM_LEVEL; }

    const std::string&  fileUrl() const { return m_fileUrl; }

    pugi::xml_document& doc() { return m_doc; }
    bool                setXmlDoc(std::vector<uint8_t> rawDoc);

    std::string         rootNs();
    std::string         selectNs();

private:
    std::string m_fileUrl;

    pugi::xml_document m_doc;
    std::vector<uint8_t> m_xmlRawBuffer; // hods the buffer for pugixml

    int m_major;
    int m_minor;

    manifest_level_t m_manifestLevel;

};

#endif // MANIFESTDOC_H
