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

#include "manifest.h"
#include "manifestdoc.h"
#include <memory>

class ManifestParser
{
private:
    typedef std::vector<uint8_t>(*DOWNLOAD_FILE_FUNCTION)(void *, std::string, long &);

public:
    ManifestParser(void *downloadFileUserPtr, DOWNLOAD_FILE_FUNCTION downloadFileFctPtr);
    bool        parse(std::string url, Manifest* manifest);
    static bool updateDvrInfo(void *downloadFileUserPtr, DOWNLOAD_FILE_FUNCTION downloadFileFctPtr,
                              const std::string &url, DvrInfo *dvrInfo);

private:
    void *m_downloadFileUserPtr;
    DOWNLOAD_FILE_FUNCTION m_downloadFileFctPtr;
    std::unique_ptr<ManifestDoc> m_f4mDoc;

    bool        initManifestParser();
    Manifest    parseManifest();
    void        parseMLStreamManifests(Manifest *manifest);
    void        parseMLStreamManifest(Manifest *manifest, Media &media);
    void        parseMedias(Manifest* manifest);
    void        parseAdaptiveSets(Manifest *manifest);
    void        parseDvrInfos(Manifest *manifest);
    void        parseDrmAdditionalHeaders(Manifest *manifest);
    void        parseBootstrapInfos(Manifest *manifest);
    void        parseSmpteTimeCodes(Manifest *manifest);
    void        parseCueInfos(Manifest *manifest);
    void        parseBestEffortFetchInfos(Manifest *manifest);
    void        parseDrmAdditionalHeaderSets(Manifest *manifest);

    // helpers
    bool        downloadF4mFile(std::vector<uint8_t> *response);
    void        setManifestVersion(std::string ns);
    void        setManifestLevel(bool isMLMStreamLevel = false);
    void        getManifestProfiles(Manifest *manifest);
    bool        nodeIsInF4mNs(const pugi::xml_node &node);
    void        forEachMedia(Manifest *manifest, std::function<void (Media &)> func);

    void        printDebugMediaCheck(const Media &media);

    static std::string  sanitizeBaseUrl(const std::string &url);

    static bool         nodeNameIs(const pugi::xpath_node &node, const char *name);
    static bool         nodeNameIs(const pugi::xml_node &node, const char *name);
    static bool         attrNameIs(const pugi::xpath_node &node, const char *name);
    static bool         attrNameIs(const pugi::xml_attribute &attr, const char *name);
    static std::string  getNodeContentAsString(const pugi::xpath_node &node);
    static int          getNodeContentAsInt(const pugi::xpath_node &node, bool *error = nullptr);
    static double       getNodeContentAsNumber(const pugi::xpath_node &node, bool *error = nullptr);
    static std::string  getAttrValueAsString(const pugi::xml_attribute &attribute);
    static int          getAttrValueAsInt(const pugi::xml_attribute &attribute,
                                          bool *error = nullptr);
    static double       getAttrValueAsNumber(const pugi::xml_attribute &attribute,
                                                     bool *error = nullptr);
    static std::vector<uint8_t>    getNodeBase64Data(const pugi::xml_node &node);
    static std::vector<uint8_t>    getNodeBase64Data(const pugi::xpath_node &node);

};

#endif // MANIFESTPARSER_H
