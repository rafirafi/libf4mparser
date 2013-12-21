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

#include <functional>

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
    void        parseMLStreamManifests(Manifest *manifest);
    void        parseMLStreamManifest(Manifest *manifest, Media &media);

    void        parseMedias(Manifest* manifest);
    void        parseDvrInfos(Manifest* manifest);
    void        parseDrmAdditionalHeaders(Manifest* manifest);
    void        parseBootstrapInfos(Manifest* manifest);

    // F4M 3.0 only
    void        parseSmpteTimeCodes(Manifest* manifest);
    void        parseCueInfos(Manifest* manifest);
    void        parseBestEffortFetchInfos(Manifest* manifest);
    void        parseDrmAdditionalHeaderSets(Manifest* manifest);
    void        parseAdaptiveSets(Manifest* manifest);
    void        getManifestProfiles(Manifest *manifest);

    void        setManifestLevel(bool isMLMStreamLevel = false);
    void        setManifestVersion(std::string ns);
    void        forEachMedia(Manifest *manifest, std::function<void(Media&)> func);
    bool        nodeIsInF4mNs(const xmlNodePtr node);

    std::string getNodeAttributeValueAsString(const xmlAttrPtr attribute);
    int         getNodeAttributeValueAsInt(const xmlAttrPtr attribute, bool *error = nullptr);
    float       getNodeAttributeValueAsFloat(const xmlAttrPtr attribute, bool *error = nullptr);

    std::string getNodeContentAsString(const xmlNodePtr node);
    int         getNodeContentAsInt(const xmlNodePtr node, bool *error = nullptr);
    float       getNodeContentAsFloat(const xmlNodePtr node, bool *error = nullptr);

    std::vector<uint8_t> getNodeBase64Data(const xmlNodePtr node);

    std::string sanitizeBaseUrl(const std::string &url);
    static bool attrNameIs(xmlAttrPtr &attribute, const char *name);
    static bool nodeNameIs(xmlNodePtr &node, const char *name);

    bool        downloadF4mFile(std::vector<uint8_t> *response);

    void printDebugMediaCheck(const Media &media);  // INFO
};

#endif // MANIFESTPARSER_H
