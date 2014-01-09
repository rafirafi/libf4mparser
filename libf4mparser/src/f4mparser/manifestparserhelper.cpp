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

#include "base64utils.h"

#include <sstream> // split string in tokens using withespace
#include <algorithm> // remove_if

#include <cstring>

#ifndef F4M_DEBUG
#define F4M_DLOG(x)
#else
#include <iostream> // std::cerr
#define F4M_DLOG(x) do { x } while(0)
#endif

bool ManifestParser::downloadF4mFile(std::vector<uint8_t> *response)
{
    if (!m_downloadFileFctPtr) {
        return false;
    }
    long status = -1;
    *response = m_downloadFileFctPtr(m_downloadFileUserPtr, m_f4mDoc->fileUrl(), status);
    if (status != 200  || response->empty()) {
        response->clear();
        F4M_DLOG(std::cerr << __func__
                 << " get manifest failed with status " << status << std::endl;);
        return false;
    }
    response->push_back('\0');

    return true;
}

void ManifestParser::setManifestVersion(std::string ns)
{
    m_f4mDoc->setVersion(ns.substr(ManifestDoc::m_nsF4mBase.size()));
    std::string query{"/manifest[@version]" + m_f4mDoc->selectNs() + "/@version"};
    pugi::string_t result = pugi::xpath_query{query.data()}.evaluate_string(m_f4mDoc->doc());
    if (!result.empty()) {
        F4M_DLOG(std::cerr << __func__ << " [version] = " << result << std::endl;);
        m_f4mDoc->setVersion(result);
    }
}

void ManifestParser::setManifestLevel(bool isMLMStreamLevel)
{
    if (isMLMStreamLevel == true) {
        return m_f4mDoc->setManifestLevel(ManifestDoc::MLM_STREAM_LEVEL);
    }
    if (m_f4mDoc->versionMajor() < 2) {
        return m_f4mDoc->setManifestLevel(ManifestDoc::SLM_STREAM_LEVEL);
    }
    std::string query{"/manifest" + m_f4mDoc->selectNs()
                + "//media[@href]" + m_f4mDoc->selectNs() + "/@href"};
    pugi::string_t result = pugi::xpath_query{query.data()}.evaluate_string(m_f4mDoc->doc());
    if (!result.empty()) {
        return m_f4mDoc->setManifestLevel(ManifestDoc::MLM_SET_LEVEL);
    }
}

void ManifestParser::getManifestProfiles(Manifest *manifest)
{
    if (m_f4mDoc->versionMajor() >= 2) {
        std::string query{"/manifest[@profile]" + m_f4mDoc->selectNs() + "/@profile"};
        pugi::string_t result = pugi::xpath_query{query.data()}.evaluate_string(m_f4mDoc->doc());
        if (!result.empty()) {
            std::string tmp;
            std::stringstream ss(result);
            while (ss >> tmp) {  // split by white space
                manifest->profiles.push_back(tmp);
            }
        }
    }
}

bool ManifestParser::nodeIsInF4mNs(const pugi::xml_node &node)
{
    return std::string{pugi::/*impl::*/namespace_uri(node)}.compare(0,
                                                   ManifestDoc::m_nsF4mBase.size(),
                                                   ManifestDoc::m_nsF4mBase) == 0;
}

// ! don't change size of medias in 'func'
void ManifestParser::forEachMedia(Manifest *manifest, std::function<void(Media&)> func)
{
    // save version in case we're modifying m_f4mDoc
    int version = m_f4mDoc->versionMajor();
    for (auto& media : manifest->medias) {
        func(media);
    }
    if (version >= 3) {
        for (auto &aSet : manifest->adaptiveSets) {
            for (auto &media : aSet.medias) {
                func(media);
            }
        }
    }
}

std::string ManifestParser::sanitizeBaseUrl(const std::string& url)
{
    std::string baseURL = url;
    if (baseURL.find_first_of('?') != std::string::npos) {
        baseURL = baseURL.substr(0, baseURL.find_first_of('?'));
    }
    if (baseURL.find_first_of('#') != std::string::npos) {
        baseURL = baseURL.substr(0, baseURL.find_first_of('#'));
    }
    baseURL = baseURL.substr(0, baseURL.find_last_of('/'));
    return baseURL;
}

void ManifestParser::printDebugMediaCheck(const Media &media)
{
    if (m_f4mDoc->isSetLevel()) {
        if (!media.bootstrapInfoId.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " bootstrapInfoId present in a set level manifest" <<std::endl;);
        }
        if (!media.drmAdditionalHeaderId.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " drmAdditionalHeaderId present in a set level manifest" <<std::endl;);
        }
        if (!media.url.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " url present in a set level manifest" <<std::endl;);
        }
        if (m_f4mDoc->versionMajor() >= 3) {
            if (!media.cueInfoId.empty()) {
                F4M_DLOG(std::cerr << __func__
                         << " cueInfoId present in a set level manifest" <<std::endl;);
            }
            if (!media.drmAdditionalHeaderSetId.empty()) {
                F4M_DLOG(std::cerr << __func__
                         << " drmAdditionalHeaderSetId present in a set level manifest"
                         <<std::endl;);
            }
        }
    }
    if (m_f4mDoc->isMultiLevelStreamLevel()) {
        if (media.alternate == true) {
            F4M_DLOG(std::cerr << __func__
                     << " alternate present in a stream level manifest" <<std::endl;);
        }
        if (!media.bitrate.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " bitrate present in a stream level manifest" <<std::endl;);
        }
        if (media.height >= 0) {
            F4M_DLOG(std::cerr << __func__
                     << " height present in a stream level manifest" <<std::endl;);
        }
        if (media.width >= 0) {
            F4M_DLOG(std::cerr << __func__
                     << " width present in a stream level manifest" <<std::endl;);
        }
        if (!media.href.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " href present in a stream level manifest" <<std::endl;);
        }
        if (!media.label.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " label present in a stream level manifest" <<std::endl;);
        }
        if (!media.lang.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " lang present in a stream level manifest" <<std::endl;);
        }
        if (!media.streamId.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " streamId present in a stream level manifest" <<std::endl;);
        }
        if (!media.type.empty()) {
            F4M_DLOG(std::cerr << __func__
                     << " type present in a stream level manifest" <<std::endl;);
        }

        if (m_f4mDoc->versionMajor() >= 3) {
            if (!media.audioCodec.empty()) {
                F4M_DLOG(std::cerr << __func__
                         << " audioCodec present in a stream level manifest" <<std::endl;);
            }
            if (!media.videoCodec.empty()) {
                F4M_DLOG(std::cerr << __func__
                         << " videoCodec present in a stream level manifest" <<std::endl;);
            }
            if (!media.bestEffortFetchInfoId.empty()) {
                F4M_DLOG(std::cerr << __func__
                         << " bestEffortFetchInfoId present in a stream level manifest"
                         <<std::endl;);
            }
        }
    }
}

bool ManifestParser::nodeNameIs(const pugi::xpath_node &node, const char *name)
{
    return strcmp(node.node().name(), name) == 0;
}

bool ManifestParser::nodeNameIs(const pugi::xml_node &node, const char *name)
{
    return strcmp(node.name(), name) == 0;
}

bool ManifestParser::attrNameIs(const pugi::xpath_node &node, const char *name)
{
    return strcmp(node.attribute().name(), name) == 0;
}

bool ManifestParser::attrNameIs(const pugi::xml_attribute &attr, const char *name)
{
    return strcmp(attr.name(), name) == 0;
}

std::string ManifestParser::getNodeContentAsString(const pugi::xpath_node &node)
{
    F4M_DLOG(std::cerr << __func__  << " [" << std::string{node.node().name()}
             << "] = " + std::string{node.node().child_value()} << std::endl;);

    return node.node().child_value();
}

int ManifestParser::getNodeContentAsInt(const pugi::xpath_node &node, bool *error)
{
    if (error) {
        *error = false;
    }
    int result = 0;
    std::string str = getNodeContentAsString(node);
    if (str.empty()) {
        if (error) {
            *error = true;
        }
        return result;
    }
    try {
        result = std::stoi(str);
    } catch (...) {
        F4M_DLOG(std::cerr << __func__
                 <<  " [" << node.node().name() << "] " << "not an int" << std::endl;);
        if (error) {
            *error = true;
        }
        result = 0;
    }

    return result;
}

double ManifestParser::getNodeContentAsNumber(const pugi::xpath_node &node, bool *error)
{
    if (error) {
        *error = false;
    }
    double result = 0.;
    std::string str = getNodeContentAsString(node);
    if (str.empty()) {
        if (error) {
            *error = true;
        }
        return result;
    }
    try {
        result = std::stod(str);
    } catch (...) {
        F4M_DLOG(std::cerr << __func__
                 <<  " [" << node.node().name() << "] " << "not a double" << std::endl;);
        if (error) {
            *error = true;
        }
        result = 0.;
    }
    return result;
}

std::string ManifestParser::getAttrValueAsString(const pugi::xml_attribute &attribute)
{
    F4M_DLOG(std::cerr << __func__  << " [" << std::string{attribute.name()}
             << "] = " + std::string{attribute.value()} << std::endl;);
    return attribute.value();
}

int ManifestParser::getAttrValueAsInt(const pugi::xml_attribute &attribute, bool *error)
{
    if (error) {
        *error = false;
    }
    int result = 0;
    std::string str = getAttrValueAsString(attribute);
    if (str.empty()) {
        if (error) {
            *error = true;
        }
        return result;
    }
    try {
        result = std::stoi(str);
    } catch (...) {
        F4M_DLOG(std::cerr << __func__
                 <<  " [" << attribute.name() << "] " << "not an int " << std::endl;);
        if (error) {
            *error = true;
        }
        result = 0;
    }

    return result;
}

double ManifestParser::getAttrValueAsNumber(const pugi::xml_attribute &attribute, bool *error)
{
    if (error) {
        *error = false;
    }
    double result = 0.;
    std::string str = getAttrValueAsString(attribute);
    if (str.empty()) {
        if (error) {
            *error = true;
        }
        return result;
    }
    try {
        result = std::stod(str);
    } catch (...) {
        F4M_DLOG(std::cerr << __func__
                 <<  " [" << attribute.name() << "] " << "not a double" << std::endl;);
        if (error) {
            *error = true;
        }
        result = 0.;
    }

    return result;
}

std::vector<uint8_t> ManifestParser::getNodeBase64Data(const pugi::xml_node &node)
{
    std::string content = node.child_value();
    content.erase(remove_if(begin(content), end(content), isspace), end(content));  // trim
    return Base64Utils::decode(content);
}

std::vector<uint8_t> ManifestParser::getNodeBase64Data(const pugi::xpath_node &node)
{
    return getNodeBase64Data(node.node());
}
