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

#ifndef MANIFESTPARSER_H
#define MANIFESTPARSER_H

#include <string>
#include <vector>
#include <memory> // unique_ptr

#include <libxml/xpath.h>

#include "manifest.h"

#include "manifestdoc.h"

class ManifestParser
{
private:
    typedef std::vector<uint8_t>(*DOWNLOAD_FILE_FUNCTION)(void *, std::string, long &);

public:
    ManifestParser(void *downloadFileUserPtr, DOWNLOAD_FILE_FUNCTION downloadFileFctPtr);
    bool        parse(std::string url, Manifest* manifest);
    static bool updateDvrInfo(void *downloadFileUserPtr, DOWNLOAD_FILE_FUNCTION downloadFileFctPtr, const std::string &url, DvrInfo *dvrInfo);

private:
    void *m_downloadFileUserPtr;
    DOWNLOAD_FILE_FUNCTION m_downloadFileFctPtr;
    std::unique_ptr<ManifestDoc> m_manifestDoc;

    bool        initManifestParser();
    Manifest    parseManifest();
    void        parseMedias(Manifest* manifest);
    void        parseDvrInfos(Manifest* manifest);
    void        parseDrmAdditionalHeaders(Manifest* manifest);
    void        parseBootstrapInfos(Manifest* manifest);

    std::string getNodeAttributeValueAsString(const xmlAttrPtr attribute);
    int         getNodeAttributeValueAsInt(const xmlAttrPtr attribute);
    float       getNodeAttributeValueAsFloat(const xmlAttrPtr attribute);

    std::string getNodeContentAsString(const xmlNodePtr node);
    int         getNodeContentAsInt(const xmlNodePtr node);
    float       getNodeContentAsFloat(const xmlNodePtr node);

    std::vector<uint8_t> getNodeBase64Data(const xmlNodePtr node);
    bool        nodeIsInNsF4m(const xmlNodePtr node);
};

#endif // MANIFESTPARSER_H
