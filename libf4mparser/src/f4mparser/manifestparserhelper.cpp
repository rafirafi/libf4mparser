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

#include <algorithm> // remove_if
#include <cstring> // strcmp
#include <sstream> // split string in tokens using withespace

#include "base64utils.h" // Base64Utils

#ifndef F4M_DEBUG
#define F4M_DLOG(x)
#else
#include <iostream> // std::cerr
#define F4M_DLOG(x) do { x } while(0)
#endif

// TODO : use number conversion error

std::string ManifestParser::getNodeAttributeValueAsString(const xmlAttrPtr attribute)
{
    std::string str;
    if (attribute && attribute->children) {
        xmlChar* attrStr = xmlNodeListGetString(m_manifestDoc->doc(), attribute->children, 1);
        if (attrStr) {
            str = (const char *)attrStr;
            str.erase(remove_if(begin(str), end(str), isspace), end(str));  // trim
            F4M_DLOG(std::cerr << __func__  << " [" << attribute->name << "] = " + str << std::endl;);
            xmlFree(attrStr);
        }
    }
    return str;
}

int ManifestParser::getNodeAttributeValueAsInt(const xmlAttrPtr attribute, bool *error)
{
    if (error) {
        *error = false;
    }
    int result = 0;
    std::string str = getNodeAttributeValueAsString(attribute);
    if (str.empty()) {
        if (error) {
            *error = true;
        }
        return result;
    }
    try {
        result = std::stoi(str);
    } catch (...) {
        F4M_DLOG(std::cerr << __func__ <<  " [" << attribute->name << "] " << "not an int " << std::endl;);
        if (error) {
            *error = true;
        }
        result = 0;
    }

    return result;
}

float ManifestParser::getNodeAttributeValueAsFloat(const xmlAttrPtr attribute, bool *error)
{
    if (error) {
        *error = false;
    }
    float result = 0.;
    std::string str = getNodeAttributeValueAsString(attribute);
    if (str.empty()) {
        if (error) {
            *error = true;
        }
        return result;
    }
    try {
        result = std::stof(str);
    } catch (...) {
        F4M_DLOG(std::cerr << __func__ <<  " [" << attribute->name << "] " << "not a float" << std::endl;);
        if (error) {
            *error = true;
        }
        result = 0.;
    }

    return result;
}

std::string ManifestParser::getNodeContentAsString(const xmlNodePtr node)
{
    std::string str;
    if (node) {
        xmlChar* content = xmlNodeGetContent(node);
        if (content) {
            str = (char *)content;
            str.erase(remove_if(begin(str), end(str), isspace), end(str));  // trim
            F4M_DLOG(std::cerr << __func__  << " [" << node->name << "] = " + str << std::endl;);
            xmlFree(content);
        }
    }
    return str;
}

int ManifestParser::getNodeContentAsInt(const xmlNodePtr node, bool *error)
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
        F4M_DLOG(std::cerr << __func__ <<  " [" << node->name << "] " << "not an int" << std::endl;);
        if (error) {
            *error = true;
        }
        result = 0;
    }

    return result;
}

float ManifestParser::getNodeContentAsFloat(const xmlNodePtr node, bool *error)
{
    if (error) {
        *error = false;
    }
    float result = 0.;
    std::string str = getNodeContentAsString(node);
    if (str.empty()) {
        if (error) {
            *error = true;
        }
        return result;
    }
    try {
        result = std::stof(str);
    } catch (...) {
        F4M_DLOG(std::cerr << __func__ <<  " [" << node->name << "] " << "not a float" << std::endl;);
        if (error) {
            *error = true;
        }
        result = 0.;
    }
    return result;
}

bool ManifestParser::nodeIsInF4mNs(const xmlNodePtr node)
{
    return (node && node->ns && node->ns->href &&
            std::string{(const char *)node->ns->href}.compare(0,
                                                              ManifestDoc::m_nsF4mBase.size(),
                                                              ManifestDoc::m_nsF4mBase) == 0);
}

std::vector<uint8_t> ManifestParser::getNodeBase64Data(const xmlNodePtr node)
{
    std::vector<uint8_t> data;
    if (node) {
        xmlChar* content = xmlNodeGetContent(node);
        if (content) {
            std::string str = (char *)content;
            xmlFree(content);
            str.erase(remove_if(begin(str), end(str), isspace), end(str));  // trim
            data = Base64Utils::decode(str);
        }
    }
    return data;
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

// ! don't change size of medias in 'func'
void ManifestParser::forEachMedia(Manifest *manifest, std::function<void(Media&)> func)
{
    // save version in case we're modifying m_manifestDoc
    int version = m_manifestDoc->versionMajor();
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

bool ManifestParser::attrNameIs(xmlAttrPtr &attribute, const char *name)
{
    return attribute && attribute->name && strcmp((const char *)attribute->name, name) == 0;
}

bool ManifestParser::nodeNameIs(xmlNodePtr &node, const char *name)
{
    return node && node->name && strcmp((const char *)node->name, name) == 0;
}

bool ManifestParser::downloadF4mFile(std::vector<uint8_t> *response)
{
    if (!m_downloadFileFctPtr) {
        return false;
    }
    long status = -1;
    *response = m_downloadFileFctPtr(m_downloadFileUserPtr, m_manifestDoc->fileUrl(), status);
    if (status != 200  || response->empty()) {
        response->clear();
        F4M_DLOG(std::cerr << __func__ << " get manifest failed with status " << status << std::endl;);
        return false;
    }
    response->push_back('\0');

    return true;
}

/*****************************************************************/
/* not realy helpers, do some parsing  */
/*****************************************************************/

void ManifestParser::setManifestLevel(bool isMLMStreamLevel)
{
    if (isMLMStreamLevel == true) {
        return m_manifestDoc->setManifestLevel(ManifestDoc::MLM_STREAM_LEVEL);
    }
    if (m_manifestDoc->versionMajor() < 2) {
        return m_manifestDoc->setManifestLevel(ManifestDoc::SLM_STREAM_LEVEL);
    }

    // check for href element
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:media[@href]");
    const xmlChar *xpathExpV3 = BAD_CAST("/nsf4m:manifest/nsf4m:media[@href] | /nsf4m:manifest/nsf4m:adaptiveSet/nsf4m:media[@href]");
    xmlXPathObjectPtr xpathObj = NULL;
    if (m_manifestDoc->versionMajor() < 3) {
        xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());
    } else {
        xpathObj = xmlXPathEvalExpression(xpathExpV3, m_manifestDoc->xpathCtx());
    }
    if (xpathObj) {
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr) {
            return m_manifestDoc->setManifestLevel(ManifestDoc::MLM_SET_LEVEL);
        }
        xmlXPathFreeObject(xpathObj);
    }

    m_manifestDoc->setManifestLevel(ManifestDoc::SLM_STREAM_LEVEL);
}

void ManifestParser::setManifestVersion(std::string ns)
{
    // set the version
    m_manifestDoc->setVersion(ns.substr(ManifestDoc::m_nsF4mBase.size()));
    // if @version exists
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest[@version]");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());
    if (xpathObj) {
        if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr) {
            m_manifestDoc->setVersion(getNodeAttributeValueAsString(xpathObj->nodesetval->nodeTab[0]->properties));
        }
        xmlXPathFreeObject(xpathObj);
    }
}

void ManifestParser::getManifestProfiles(Manifest *manifest)
{
    if (m_manifestDoc->versionMajor() >= 2) {
        const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest[@profile]");
        xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());
        if (xpathObj) {
            if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr) {
                std::string profiles = getNodeAttributeValueAsString(xpathObj->nodesetval->nodeTab[0]->properties);
                if (!profiles.empty()) {
                    std::string tmp;
                    std::stringstream ss(profiles);
                    while (ss >> tmp) {
                        manifest->profiles.push_back(tmp);
                    }
                }
            }
            xmlXPathFreeObject(xpathObj);
        }
    }
}
