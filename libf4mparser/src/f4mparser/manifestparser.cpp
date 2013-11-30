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

#include <algorithm> // sort
#include <cstring> // strcmp

#include <libxml/xpathInternals.h> // xmlXpathRegisterNS

#include "urlutils.h" // UrlUtils

#ifndef F4M_DEBUG
#define F4M_DLOG(x)
#else
#include <iostream> // std::cerr
#define F4M_DLOG(x) x
#endif

// secure a little the access to underlying vector ptr
template <class T, class TAl>
inline T* begin_ptr(std::vector<T,TAl>& v)
{return  v.empty() ? nullptr : &v[0];}

ManifestParser::ManifestParser(void *downloadFileUserPtr, DOWNLOAD_FILE_FUNCTION downloadFileFctPtr)
    : m_downloadFileUserPtr(downloadFileUserPtr), m_downloadFileFctPtr(downloadFileFctPtr)
{
}

bool ManifestParser::parse(std::string url, Manifest *manifest)
{
    m_manifestDoc = std::unique_ptr<ManifestDoc>(new ManifestDoc{url});

    // prepare to parse
    if (!initManifestParser()) {
        return false;
    }
    F4M_DLOG(std::cerr << " f4m namespace : " << m_manifestDoc->m_nsF4m << std::endl;)

    // parse manifest
    *manifest = parseManifest();

    // parse MLM : got get the stream-level manifest(s) if necessary
    if (m_manifestDoc->getF4mVersion() == 2) {

        for (auto &media : manifest->medias) {

            if (!media.href.empty()) {

                // init parser
                m_manifestDoc.reset(new ManifestDoc{media.href});
                m_manifestDoc->m_isMultiLevelStreamLevel = true;
                if (!initManifestParser()) {
                    F4M_DLOG(std::cerr << "initParser for stream-level manifest failed" << std::endl;)
                    continue;
                }

                Manifest subManifest = parseManifest();

                // these values should be read only from the set-level manifest
                subManifest.medias.at(0).width = media.width;
                subManifest.medias.at(0).height = media.height;
                subManifest.medias.at(0).alternate = media.alternate;
                if (!media.type.empty()) {
                    subManifest.medias.at(0).type = media.type;
                } else {
                    subManifest.medias.at(0).type.clear();
                }
                if (!media.label.empty()) {
                    subManifest.medias.at(0).label = media.label;
                } else {
                    subManifest.medias.at(0).label.clear();
                }
                if (!media.lang.empty()) {
                    subManifest.medias.at(0).lang = media.lang;
                } else {
                    subManifest.medias.at(0).lang.clear();
                }
                if (!media.bitrate.empty()) {
                    subManifest.medias.at(0).bitrate = media.bitrate;
                } else {
                    subManifest.medias.at(0).bitrate.clear();
                }

                // pass the dvrInfo to the media
                subManifest.medias.at(0).dvrInfo = media.dvrInfo;

                // replace media
                media = subManifest.medias.at(0);
            }
        }
    }

    return true;
}

bool ManifestParser::initManifestParser()
{
    if (m_manifestDoc->m_fileUrl.empty()) {
        F4M_DLOG(std::cerr << __func__ << " manifest url empty " << std::endl;)
                return false;
    }

    // only http url for now
    if (UrlUtils::haveHttpScheme(m_manifestDoc->m_fileUrl) == false) {
        F4M_DLOG(std::cerr << __func__ << " manifest url scheme don't begin with http " << std::endl;)
        return false;
    }

    // get xml file
    if (!m_downloadFileFctPtr) {
        return false;
    }
    long status = -1;
    std::vector<uint8_t> response = m_downloadFileFctPtr(m_downloadFileUserPtr,
                                                         m_manifestDoc->m_fileUrl, status);
    if (status != 200  || response.empty()) {
        response.clear();
        F4M_DLOG(std::cerr << __func__ << " get manifest failed with status " << status << std::endl;)
        return false;
    }
    response.push_back('\0');

    // prepare ManifestDoc for parsing
    m_manifestDoc->m_doc = xmlReadMemory(reinterpret_cast<const char *>(begin_ptr(response)),
                                         static_cast<int>(response.size()),
                                         "manifest.f4m", NULL, 0);
    if (!m_manifestDoc->m_doc) {
        F4M_DLOG(std::cerr << __func__ << " xmlReadMemory failed" << std::endl;)
        return false;
    }

    // get ns
    xmlNsPtr ns = xmlSearchNs(m_manifestDoc->m_doc,
                              xmlDocGetRootElement(m_manifestDoc->m_doc), NULL);
    if (ns && ns->href && strcmp((const char *)ns->href, ManifestDoc::m_nsF4mVersion1) == 0) {
        m_manifestDoc->m_nsF4m = ManifestDoc::m_nsF4mVersion1;
    } else if (ns && ns->href && strcmp((const char *)ns->href, ManifestDoc::m_nsF4mVersion2) == 0) {
        m_manifestDoc->m_nsF4m = ManifestDoc::m_nsF4mVersion2;
    } else {
        F4M_DLOG(std::cerr << __func__ << " find f4m namespace failed" << std::endl;)
        return false;
    }

    // create a xpath evaluation context
    m_manifestDoc->m_xpathCtx = xmlXPathNewContext(m_manifestDoc->m_doc);
    if (!m_manifestDoc->m_xpathCtx) {
        F4M_DLOG(std::cerr << __func__ << " xmlXPathNewContext failed" << std::endl;)
        return false;
    }

    // register namespace
    if (xmlXPathRegisterNs(m_manifestDoc->m_xpathCtx, (const xmlChar*)"nsf4m",
                           (const xmlChar*)m_manifestDoc->m_nsF4m)) {
        F4M_DLOG(std::cerr << __func__ << " xmlXPathRegisterNs failed" << std::endl;)
        return false;
    }

    return true;
}

Manifest ManifestParser::parseManifest()
{
    Manifest manifest;

    // get manifest child in the nsf4m ns only
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)"/nsf4m:manifest/nsf4m:*",
                                                        m_manifestDoc->m_xpathCtx);

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (int i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];

                if (node && node->type == XML_ELEMENT_NODE && node->name) {
                    if (strcmp((const char *)node->name, "baseURL") == 0) {
                        manifest.baseURL = getNodeContentAsString(node);
                    } else if (strcmp((const char *)node->name, "startTime") == 0) {
                        manifest.startTime = getNodeContentAsString(node);
                    } else if (strcmp((const char *)node->name, "mimeType") == 0) {
                        manifest.mimeType = getNodeContentAsString(node);
                    } else if (strcmp((const char *)node->name, "streamType") == 0) {
                        manifest.streamType = getNodeContentAsString(node);
                    } else if (strcmp((const char *)node->name, "deliveryType") == 0) {
                        manifest.deliveryType = getNodeContentAsString(node);
                    } else if (strcmp((const char *)node->name, "label") == 0) {
                        manifest.label = getNodeContentAsString(node);
                    } else if (strcmp((const char *)node->name, "id") == 0) {
                        manifest.id = getNodeContentAsString(node);
                    } else if (strcmp((const char *)node->name, "lang") == 0) {
                        manifest.lang = getNodeContentAsString(node);
                    }  else if (strcmp((const char *)node->name, "duration") == 0) {
                        manifest.duration = getNodeContentAsFloat(node);
                    } else if (strcmp((const char *)node->name, "media")
                               && strcmp((const char *)node->name, "bootstrapInfo")
                               && strcmp((const char *)node->name, "dvrInfo")
                               && strcmp((const char *)node->name, "drmAdditionalHeader")) {
                        F4M_DLOG(std::cerr << __func__ <<  " : node : ["
                                 << (const char *)node->name << "] ignored" << std::endl;)
                        getNodeContentAsString(node);
                    }
                }
            }

        }

        if (manifest.baseURL.empty()) {
            manifest.baseURL = m_manifestDoc->m_fileUrl;
            // sanitize
            if (manifest.baseURL.find_first_of('?') != std::string::npos) {
                manifest.baseURL = manifest.baseURL.substr(0, manifest.baseURL.find_first_of('?'));
            }
            // eventually
            if (manifest.baseURL.find_first_of('#') != std::string::npos) {
                manifest.baseURL = manifest.baseURL.substr(0, manifest.baseURL.find_first_of('#'));
            }
            manifest.baseURL = manifest.baseURL.substr(0, manifest.baseURL.find_last_of('/'));
        }

        xmlXPathFreeObject(xpathObj);
    }

    // then get the medias : download now if MLM set-level manifest
    parseMedias(&manifest);

    // get the dvrInfo
    if (m_manifestDoc->m_isMultiLevelStreamLevel == false) {
        parseDvrInfos(&manifest);
    }

    // get the drmAdditionalHeaders
    parseDrmAdditionalHeaders(&manifest);

    // then get the bootstrapInfo(s) and assign them to media : don't download from @url
    parseBootstrapInfos(&manifest);

    return manifest;
}

void ManifestParser::parseMedias(Manifest* manifest)
{
    // get all the medias
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)"/nsf4m:manifest/nsf4m:media",
                                                        m_manifestDoc->m_xpathCtx);
    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (int i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                Media media;
                //get all the attributes
                for (xmlAttrPtr attribute = node->properties;
                     attribute != NULL; attribute = attribute->next) {

                    if (!attribute->name) {
                        continue;
                    }

                    if (m_manifestDoc->getF4mVersion() == 1) {
                        if (strcmp((const char *)attribute->name, "dvrInfoId") == 0) {
                            media.dvrInfoId = getNodeAttributeValueAsString(attribute);
                            continue;
                        }

                    } else if (m_manifestDoc->getF4mVersion() == 2) {
                        if (strcmp((const char *)attribute->name, "href") == 0) {
                            media.href = getNodeAttributeValueAsString(attribute);
                            if (UrlUtils::isAbsolute(media.href) == false) {
                                media.href.insert(0, manifest->baseURL + "/");
                            }
                            continue;
                        }
                    }

                    // bitrate, streamId, width, height, type, alternate, label, lang
                    if (m_manifestDoc->m_isMultiLevelStreamLevel == false) {

                        if (strcmp((const char *)attribute->name, "bitrate") == 0) {
                            media.bitrate = getNodeAttributeValueAsString(attribute);
                            continue;
                        } else if (strcmp((const char *)attribute->name, "streamId") == 0) {
                            media.streamId = getNodeAttributeValueAsString(attribute);
                            continue;
                        } else if (strcmp((const char *)attribute->name, "width") == 0) {
                            media.width = getNodeAttributeValueAsInt(attribute);
                            continue;
                        } else if (strcmp((const char *)attribute->name, "height") == 0) {
                            media.height = getNodeAttributeValueAsInt(attribute);
                            continue;
                        } else if (strcmp((const char *)attribute->name, "type") == 0) {
                            media.type = getNodeAttributeValueAsString(attribute);
                            continue;
                        } else if (strcmp((const char *)attribute->name, "alternate") == 0) {
                            media.alternate = true;
                            continue;
                        } else if (strcmp((const char *)attribute->name, "label") == 0) {
                            media.label = getNodeAttributeValueAsString(attribute);
                            continue;
                        } else if (strcmp((const char *)attribute->name, "lang") == 0) {
                            media.lang = getNodeAttributeValueAsString(attribute);
                            continue;
                        }

                    }

                    if (strcmp((const char *)attribute->name, "url") == 0) {
                        media.url = getNodeAttributeValueAsString(attribute);
                        if (UrlUtils::isAbsolute(media.url) == false) {
                            media.url.insert(0, manifest->baseURL + "/");
                        }
                    } else if (strcmp((const char *)attribute->name, "bootstrapInfoId") == 0) {
                        media.bootstrapInfoId = getNodeAttributeValueAsString(attribute);
                    } else if (strcmp((const char *)attribute->name, "drmAdditionalHeaderId") == 0) {
                        media.drmAdditionalHeaderId = getNodeAttributeValueAsString(attribute);
                    } else if (strcmp((const char *)attribute->name, "groupspec") == 0) {
                        media.groupspec = getNodeAttributeValueAsString(attribute);
                    } else if (strcmp((const char *)attribute->name, "multicastStreamName") == 0) {
                        media.multicastStreamName = getNodeAttributeValueAsString(attribute);
                    }   else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attribute "
                                 << (const char *)attribute->name << " ignored" << std::endl;)
                        getNodeAttributeValueAsString(attribute);
                    }

                }
                // get child elements
                for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
                    // check we don't escape ns
                    if (child->type == XML_ELEMENT_NODE && nodeIsInNsF4m(child) && child->name) {

                        if (m_manifestDoc->getF4mVersion() == 1) {
                            if (strcmp((const char*)child->name, "moov") == 0) {
                                media.moov = getNodeBase64Data(child);
                                continue;
                            } else if (strcmp((const char*)child->name, "xmpMetadata") == 0) {
                                media.xmpMetadata = getNodeBase64Data(child);
                                continue;
                            }
                        }

                        if (strcmp((const char*)child->name, "metadata") == 0) {
                            media.metadata = getNodeBase64Data(child);
                        }

                    }
                }

                // check
                if (!media.groupspec.empty() || !media.multicastStreamName.empty()) {
                    if ((media.groupspec.empty() != media.multicastStreamName.empty()) ||
                            UrlUtils::haveRtmfpScheme(media.url) == false) {
                        F4M_DLOG(std::cerr << __func__ << " multicast for rtmfp not valid " <<std::endl;)
                        continue;
                    }
                }

                // push to manifest
                manifest->medias.push_back(media);

                // check only one for MultiLevel StreamLevel
                if (m_manifestDoc->m_isMultiLevelStreamLevel) {
                    break;
                }
            }

        }

        xmlXPathFreeObject(xpathObj);
    }

}

void ManifestParser::parseBootstrapInfos(Manifest *manifest)
{
    // get all the bootstrapInfos
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)"/nsf4m:manifest/nsf4m:bootstrapInfo",
                                                        m_manifestDoc->m_xpathCtx);

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                BootstrapInfo bootstrapInfo;

                for (xmlAttrPtr attribute = node->properties;
                     attribute != NULL; attribute = attribute->next) {
                    if (!attribute->name) {
                        continue;
                    }
                    if (strcmp((const char *)attribute->name, "profile") == 0) {
                        bootstrapInfo.profile = getNodeAttributeValueAsString(attribute);//mandatory
                    } else if (strcmp((const char *)attribute->name, "id") == 0) {
                        bootstrapInfo.id = getNodeAttributeValueAsString(attribute);
                    } else if (strcmp((const char *)attribute->name, "url") == 0) {
                        bootstrapInfo.url = getNodeAttributeValueAsString(attribute);
                        if (UrlUtils::isAbsolute(bootstrapInfo.url) == false) {
                            bootstrapInfo.url.insert(0, manifest->baseURL + "/");
                        }
                    } else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attribute " << (const char *)attribute->name
                                 << " ignored" << std::endl;)
                        getNodeAttributeValueAsString(attribute); // D
                    }
                }

                // check
                if (bootstrapInfo.profile.empty()) {
                    F4M_DLOG(std::cerr << __func__ << " ignoring malformed bootstrap : no profile attribute"
                             << std::endl;)
                    continue;
                }

                // get the data if here : the content of a node is a child of a node
                if (bootstrapInfo.url.empty()) {
                    bootstrapInfo.data = getNodeBase64Data(node);
                    // check
                    if (bootstrapInfo.data.empty()) {
                        F4M_DLOG(std::cerr << __func__ << "ignoring malformed bootstrap : no data"
                                 << std::endl;)
                        continue;
                    }
                }

                // assign the bootstrap to medias
                for (auto& media : manifest->medias) {
                    if (media.bootstrapInfoId.empty() ||
                            media.bootstrapInfoId == bootstrapInfo.id) {
                        media.bootstrapInfo = bootstrapInfo;
                    }
                }

            }

        }

        xmlXPathFreeObject(xpathObj);
    }

}

void ManifestParser::parseDvrInfos(Manifest *manifest)
{
    // get all the dvrInfos
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression((const xmlChar *)"/nsf4m:manifest/nsf4m:dvrInfo",
                                                        m_manifestDoc->m_xpathCtx);

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                DvrInfo dvrInfo;
                //get all the attributes
                for (xmlAttrPtr attribute = node->properties;
                     attribute != NULL; attribute = attribute->next) {

                    if (!attribute->name) {
                        continue;
                    }

                    if (m_manifestDoc->getF4mVersion() == 1) {
                        if (strcmp((const char *)attribute->name, "id") == 0) {
                            dvrInfo.id = getNodeAttributeValueAsString(attribute);
                            continue;
                        } else if (strcmp((const char *)attribute->name, "beginOffset") == 0) {
                            dvrInfo.beginOffset = getNodeAttributeValueAsInt(attribute);
                            continue;
                        } else if (strcmp((const char *)attribute->name, "endOffset") == 0) {
                            dvrInfo.endOffset = getNodeAttributeValueAsInt(attribute);
                            continue;
                        }

                    } else if (m_manifestDoc->getF4mVersion() == 2) {
                        if (strcmp((const char *)attribute->name, "windowDuration") == 0) {
                            dvrInfo.windowDuration = getNodeAttributeValueAsInt(attribute);
                            continue;
                        }
                    }

                    if (strcmp((const char *)attribute->name, "url") == 0) {
                        dvrInfo.url = getNodeAttributeValueAsString(attribute);
                        if (UrlUtils::isAbsolute(dvrInfo.url) == false) {
                            dvrInfo.url.insert(0, manifest->baseURL + "/");
                        }
                    } else if (strcmp((const char *)attribute->name, "offline") == 0) {
                        dvrInfo.offline = true;
                    } else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attribute " <<(const char *)attribute->name
                                 << " ignored" << std::endl;)
                        getNodeAttributeValueAsString(attribute); // D
                    }

                } // for

                // assign the dvrInfo to medias
                for (auto& media : manifest->medias) {

                    if (m_manifestDoc->getF4mVersion() == 2 ||
                            (media.dvrInfoId.empty() || media.dvrInfoId == dvrInfo.id)) {
                        media.dvrInfo = dvrInfo;
                    }
                }

            } //for

        }

        xmlXPathFreeObject(xpathObj);
    } //xpathObj

}

void ManifestParser::parseDrmAdditionalHeaders(Manifest *manifest)
{

    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(
                (const xmlChar *)"/nsf4m:manifest/nsf4m:drmAdditionalHeader",
                m_manifestDoc->m_xpathCtx);

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                DrmAdditionalHeader drmAdditionalHeader;

                for (xmlAttrPtr attribute = node->properties;
                     attribute != NULL; attribute = attribute->next) {
                    if (!attribute->name) {
                        continue;
                    }
                    if (strcmp((const char *)attribute->name, "id") == 0) {
                        drmAdditionalHeader.id = getNodeAttributeValueAsString(attribute);
                    } else if (strcmp((const char *)attribute->name, "drmContentId") == 0) {
                        drmAdditionalHeader.drmContentId = getNodeAttributeValueAsString(attribute);
                    } else if (strcmp((const char *)attribute->name, "url") == 0) {
                        drmAdditionalHeader.url = getNodeAttributeValueAsString(attribute);
                        if (UrlUtils::isAbsolute(drmAdditionalHeader.url) == false) {
                            drmAdditionalHeader.url.insert(0, manifest->baseURL + "/");
                        }
                    } else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attribute " << (const char *)attribute->name
                                 << " ignored" << std::endl;)
                        getNodeAttributeValueAsString(attribute); // D
                    }
                }

                // get the data if here : the content of a node is a child of a node
                if (drmAdditionalHeader.url.empty()) {
                    drmAdditionalHeader.data = getNodeBase64Data(node);
                    // check
                    if (drmAdditionalHeader.data.empty()) {
                        F4M_DLOG(std::cerr << __func__ << "ignoring malformed bootstrap : no data"
                                 << std::endl;)
                        continue;
                    }
                }

                // assign the bootstrap to medias
                for (auto& media : manifest->medias) {
                    if (media.drmAdditionalHeaderId.empty() ||
                            media.drmAdditionalHeaderId == drmAdditionalHeader.id) {
                        media.drmAdditionalHeader = drmAdditionalHeader;
                    }
                }

            }

        }

        xmlXPathFreeObject(xpathObj);
    }
}

bool ManifestParser::updateDvrInfo(void *downloadFileUserPtr,
                                   DOWNLOAD_FILE_FUNCTION downloadFileFctPtr,
                                   const std::string &url,
                                   DvrInfo *dvrInfo)
{
    bool result = false;
    long status = -1;
    std::vector<uint8_t> response;
    xmlDocPtr doc = NULL;
    xmlNodePtr root = NULL;


    // helpers
    auto getAttributeValueAsString = [&](const xmlAttr* attribute) {
        std::string str;
        if (attribute && attribute->children) {
            xmlChar* attrStr = xmlNodeListGetString(doc, attribute->children, 1);
            if (attrStr) {
                str = (const char *)attrStr;
                F4M_DLOG(std::cerr << __func__  << " [" << attribute->name << "] = " + str << std::endl;)
                xmlFree(attrStr);
            }
        }
        return str;
    };

    auto getAttributeValueAsInt = [&](const xmlAttr* attribute) {
        int res = 0;
        std::string str = getAttributeValueAsString(attribute);
        if (str.empty()) {
            return res;
        }
        try { res = std::stoi(str); } catch (...) {
            F4M_DLOG(std::cerr << __func__ <<  " [" << attribute->name << "] " << "not an int "
                     << res << std::endl;)
                    res = 0; } // !
        return res;
    };

    // check we can download
    if (!downloadFileUserPtr || url.empty() || !UrlUtils::haveHttpScheme(url)) {
        F4M_DLOG(std::cerr << __func__ << " unable to download from url" << std::endl;)
        goto fail;
    }

    // get xml file : I'm not sure for authority, set it to false
    response = downloadFileFctPtr(downloadFileUserPtr, url, status );
    if (status != 200 || response.empty()) {
        response.clear();
        F4M_DLOG(std::cerr << __func__ << " get dvrInfo failed with status " << status << std::endl;)
        goto fail;
    }
    response.push_back('\0');

    // get xml doc
    doc = xmlReadMemory(reinterpret_cast<const char *>(begin_ptr(response)),
                        (int)response.size(), "manifest.f4m", NULL, 0);
    if (!doc) {
        F4M_DLOG(std::cerr << __func__ << " xmlReadMemory failed" << std::endl;)
        goto fail;
    }

    // get root
    root = xmlDocGetRootElement(doc);
    if (!root) {
        F4M_DLOG(std::cerr << __func__ << " xmlDocGetRootElement failed" << std::endl;)
        goto fail;
    }

    // check tag
    if (strcmp((const char *)root->name, "dvrInfo")) {
        F4M_DLOG(std::cerr << __func__ << " root tag is not dvrInfo failed" << std::endl;)
        goto fail;
    }

    // get attributes minus url
    for (xmlAttrPtr attribute = root->properties; attribute != NULL; attribute = attribute->next) {
        if (!attribute->name) {
            continue;
        }
        if (strcmp((const char *)attribute->name, "id") == 0) {
            dvrInfo->id = getAttributeValueAsString(attribute);
        } else if (strcmp((const char *)attribute->name, "beginOffset") == 0) {
            dvrInfo->beginOffset = getAttributeValueAsInt(attribute);
        } else if (strcmp((const char *)attribute->name, "endOffset") == 0) {
            dvrInfo->endOffset = getAttributeValueAsInt(attribute);
        }else if (strcmp((const char *)attribute->name, "windowDuration") == 0) {
            dvrInfo->windowDuration = getAttributeValueAsInt(attribute);
        } else if (strcmp((const char *)attribute->name, "offline") == 0) {
            dvrInfo->offline = true;
        } else {
            F4M_DLOG(std::cerr << __func__ <<  " : attribute " <<(const char *)attribute->name
                     << " ignored" << std::endl;)
            getAttributeValueAsString(attribute); // D
        }

    }

    result = true;

fail:
    if (doc) {
        xmlFreeDoc(doc);
        doc = NULL;
    }

    xmlCleanupParser();

    return result;
}
