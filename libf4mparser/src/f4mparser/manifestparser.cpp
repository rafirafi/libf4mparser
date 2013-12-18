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

#include <algorithm>  // sort
#include <cstring>  // strcmp

#include <libxml/xpathInternals.h> // xmlXpathRegisterNS

#include "urlutils.h"  // UrlUtils

#ifndef F4M_DEBUG
#define F4M_DLOG(x)
#else
#include <iostream>  // std::cerr
#define F4M_DLOG(x) do { x } while(0)
#endif

//16 Dec 2013 : I think parsing order is ok, retrieval of content is ok in itself,
// globally for f4M spec < 3.0 it's usable
// what to rework first :  - the way the info is stored (duplication+++), no easy way to know if an
//                              element was present in the xml file
//                          - some conditionnal may be wrong / don't respect spec 3.0
// It can be interesting to rework the xsd shema in spec 3.0 to make it parsable by libxml2.


ManifestParser::ManifestParser(void *downloadFileUserPtr, DOWNLOAD_FILE_FUNCTION downloadFileFctPtr)
    : m_downloadFileUserPtr(downloadFileUserPtr), m_downloadFileFctPtr(downloadFileFctPtr)
{
}

bool ManifestParser::parse(std::string url, Manifest *manifest)
{
    m_manifestDoc = std::unique_ptr<ManifestDoc>(new ManifestDoc{url});

    if (!initManifestParser()) {
        return false;
    }

    setManifestLevel();

    *manifest = parseManifest();

    if (m_manifestDoc->isSetLevel()) {
        parseMLStreamManifests(manifest);
    }

    return true;
}

bool ManifestParser::initManifestParser()
{
    if (m_manifestDoc->fileUrl().empty()) {
        F4M_DLOG(std::cerr << __func__ << " manifest url empty" << std::endl;);
        return false;
    }

    if (UrlUtils::haveHttpScheme(m_manifestDoc->fileUrl()) == false) {
        F4M_DLOG(std::cerr << __func__ << " manifest url scheme don't begin with http" << std::endl;);
        return false;
    }

    std::vector<uint8_t> response;
    if (downloadF4mFile(&response) == false) {
        F4M_DLOG(std::cerr << __func__ << " failed to dowload manifest" << std::endl;);
        return false;
    }
    // response not empty : &response[0] defined
    m_manifestDoc->setDoc(xmlReadMemory(reinterpret_cast<const char *>(&response[0]),
                                        static_cast<int>(response.size()),
                                        "manifest.f4m", NULL, 0));
    if (!m_manifestDoc->doc()) {
        F4M_DLOG(std::cerr << __func__ << " xmlReadMemory failed" << std::endl;);
        return false;
    }

    m_manifestDoc->setXpathCtx(xmlXPathNewContext(m_manifestDoc->doc()));
    if (!m_manifestDoc->xpathCtx()) {
        F4M_DLOG(std::cerr << __func__ << " xmlXPathNewContext failed" << std::endl;);
        return false;
    }

    xmlNsPtr ns = xmlSearchNs(m_manifestDoc->doc(), xmlDocGetRootElement(m_manifestDoc->doc()), NULL);
    if (!ns || !ns->href || std::string{(const char *)ns->href}.compare(0,
                                                                        ManifestDoc::m_nsF4mBase.size(),
                                                                        ManifestDoc::m_nsF4mBase) != 0) {
        F4M_DLOG(std::cerr << __func__ << " can't find f4m ns" << std::endl;);
        return false;
    }

    if (xmlXPathRegisterNs(m_manifestDoc->xpathCtx(), BAD_CAST("nsf4m"), ns->href)) {
        F4M_DLOG(std::cerr << __func__ << " xmlXPathRegisterNs failed" << std::endl;);
        return false;
    }

    setManifestVersion((const char *)ns->href);

    return true;
}

Manifest ManifestParser::parseManifest()
{
    Manifest manifest;

    if (m_manifestDoc->versionMajor() >= 2) {
        getManifestProfiles(&manifest);
    }

    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:*");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (int i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];

                if (node && node->type == XML_ELEMENT_NODE && node->name) {
                    if (nodeNameIs(node, "baseURL")) {
                        manifest.baseURL = getNodeContentAsString(node);
                    } else if (nodeNameIs(node, "startTime")) {
                        manifest.startTime = getNodeContentAsString(node);
                    } else if (nodeNameIs(node, "mimeType")) {
                        manifest.mimeType = getNodeContentAsString(node);
                    } else if (nodeNameIs(node, "streamType")) {
                        manifest.streamType = getNodeContentAsString(node);
                    } else if (nodeNameIs(node, "deliveryType")) {
                        manifest.deliveryType = getNodeContentAsString(node);
                    } else if (nodeNameIs(node, "label")) {
                        manifest.label = getNodeContentAsString(node);
                    } else if (nodeNameIs(node, "id")) {
                        manifest.id = getNodeContentAsString(node);
                    } else if (nodeNameIs(node, "lang")) {
                        manifest.lang = getNodeContentAsString(node);
                    }  else if (nodeNameIs(node, "duration")) {
                        manifest.duration = getNodeContentAsFloat(node);
                    }
                    // report element we don't parse
                    else if (nodeNameIs(node, "media")
                             && nodeNameIs(node, "bootstrapInfo")
                             && nodeNameIs(node, "dvrInfo")
                             && nodeNameIs(node, "drmAdditionalHeader")
                             && nodeNameIs(node, "smpteTimecodes")
                             && nodeNameIs(node, "cueInfo")
                             && nodeNameIs(node, "bestEffortFetchInfo")
                             && nodeNameIs(node, "drmAdditionalHeaderSet")
                             && nodeNameIs(node, "adaptiveSet")) {
                        F4M_DLOG(std::cerr << __func__ <<  " : node : ["
                                 << (const char *)node->name << "] ignored" << std::endl;);
                        getNodeContentAsString(node);
                    }
                }
            }
        }

        if (manifest.baseURL.empty()) {
            manifest.baseURL = sanitizeBaseUrl(m_manifestDoc->fileUrl());
        }

        xmlXPathFreeObject(xpathObj);
    }

    parseMedias(&manifest);

    if (m_manifestDoc->versionMajor() >= 3 && m_manifestDoc->isMultiLevelStreamLevel() == false) {
        parseAdaptiveSet(&manifest);
    }

    if (m_manifestDoc->isMultiLevelStreamLevel() == false) {
        parseDvrInfos(&manifest);
    }

    parseDrmAdditionalHeaders(&manifest);

    parseBootstrapInfos(&manifest);

    if (m_manifestDoc->versionMajor() >= 3 ) {
        if (m_manifestDoc->isSetLevel() == false) {
            parseSmpteTimeCodes(&manifest);
            parseCueInfos(&manifest);
        }

        if (m_manifestDoc->isSetLevel() == true) {
            parseBestEffortFetchInfo(&manifest);
        }

        parseDrmAdditionalHeaderSet(&manifest);
    }

    return manifest;
}

void ManifestParser::parseMLStreamManifests(Manifest *manifest)
{
    auto doParse = [&](Media &media) {
        parseMLStreamManifest(manifest, media);
    };
    forEachMedia(manifest, doParse);
}

void ManifestParser::parseMLStreamManifest(Manifest *manifest, Media &media)
{
    m_manifestDoc.reset(new ManifestDoc{media.href});

    if (!initManifestParser()) {
        F4M_DLOG(std::cerr << "initParser for ML stream-level manifest failed" << std::endl;);
        return;
    }

    setManifestLevel(true);

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
    // TODO find a better way to do this
    subManifest.medias.at(0).dvrInfo = media.dvrInfo;

    // replace media
    media = subManifest.medias.at(0);

    // TODO: F4M 3.0 not really sure how to deal with profiles
    for (auto profile : subManifest.profiles) {
        manifest->profiles.push_back(profile);
    }
}

void ManifestParser::parseMedias(Manifest* manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:media");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (int i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                Media media;

                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {

                    if (!attr->name) {
                        continue;
                    }

                    if (m_manifestDoc->versionMajor() == 1) {
                        if (attrNameIs(attr, "dvrInfoId")) {
                            media.dvrInfoId = getNodeAttributeValueAsString(attr);
                            continue;
                        }

                    } else if (m_manifestDoc->versionMajor() >= 2) {
                        if (attrNameIs(attr, "href")) {
                            media.href = getNodeAttributeValueAsString(attr);
                            if (UrlUtils::isAbsolute(media.href) == false) {
                                media.href.insert(0, manifest->baseURL + "/");
                            }
                            continue;
                        }

                        if (m_manifestDoc->versionMajor() >= 3) {
                            if (attrNameIs(attr, "audioCodec")) {
                                media.audioCodec = getNodeAttributeValueAsString(attr);
                                continue;
                            } else if (attrNameIs(attr, "videoCodec")) {
                                media.videoCodec = getNodeAttributeValueAsString(attr);
                                continue;
                            }
                            else if (attrNameIs(attr, "cueInfoId")) {
                                media.cueInfoId = getNodeAttributeValueAsString(attr);
                                continue;
                            }
                            else if (attrNameIs(attr, "bestEffortFetchInfoId")) {
                                media.bestEffortFetchInfoId = getNodeAttributeValueAsString(attr);
                                continue;
                            }
                            else if (attrNameIs(attr, "drmAdditionalHeaderSetId")) {
                                media.drmAdditionalHeaderSetId = getNodeAttributeValueAsString(attr);
                                continue;
                            }
                        }
                    }

                    if (m_manifestDoc->isMultiLevelStreamLevel() == false) {

                        if (attrNameIs(attr, "bitrate")) {
                            media.bitrate = getNodeAttributeValueAsString(attr);
                            continue;
                        } else if (attrNameIs(attr, "streamId")) {
                            media.streamId = getNodeAttributeValueAsString(attr);
                            continue;
                        } else if (attrNameIs(attr, "width")) {
                            media.width = getNodeAttributeValueAsInt(attr);
                            continue;
                        } else if (attrNameIs(attr, "height")) {
                            media.height = getNodeAttributeValueAsInt(attr);
                            continue;
                        } else if (attrNameIs(attr, "type")) {
                            media.type = getNodeAttributeValueAsString(attr);
                            continue;
                        } else if (attrNameIs(attr, "alternate")) {
                            media.alternate = true;
                            continue;
                        } else if (attrNameIs(attr, "label")) {
                            media.label = getNodeAttributeValueAsString(attr);
                            continue;
                        } else if (attrNameIs(attr, "lang")) {
                            media.lang = getNodeAttributeValueAsString(attr);
                            continue;
                        }
                    }

                    if (attrNameIs(attr, "url")) {
                        media.url = getNodeAttributeValueAsString(attr);
                        if (UrlUtils::isAbsolute(media.url) == false) {
                            media.url.insert(0, manifest->baseURL + "/");
                        }
                    } else if (attrNameIs(attr, "bootstrapInfoId")) {
                        media.bootstrapInfoId = getNodeAttributeValueAsString(attr);
                    } else if (attrNameIs(attr, "drmAdditionalHeaderId")) {
                        media.drmAdditionalHeaderId = getNodeAttributeValueAsString(attr);
                    } else if (attrNameIs(attr, "groupspec")) {
                        media.groupspec = getNodeAttributeValueAsString(attr);
                    } else if (attrNameIs(attr, "multicastStreamName")) {
                        media.multicastStreamName = getNodeAttributeValueAsString(attr);
                    }   else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attr "
                                 << (const char *)attr->name << " ignored" << std::endl;);
                        getNodeAttributeValueAsString(attr);
                    }
                }

                for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
                    // check we don't escape ns
                    if (child->type == XML_ELEMENT_NODE && nodeIsInF4mNs(child) && child->name) {

                        if (m_manifestDoc->versionMajor() == 1) {
                            if (nodeNameIs(child, "moov")) {
                                media.moov = getNodeBase64Data(child);
                                continue;
                            } else if (nodeNameIs(child, "xmpMetadata")) {
                                media.xmpMetadata = getNodeBase64Data(child);
                                continue;
                            }
                        }

                        if (nodeNameIs(child, "metadata")) {
                            media.metadata = getNodeBase64Data(child);
                        }
                    }
                }

                // check
                if (!media.groupspec.empty() || !media.multicastStreamName.empty()) {
                    if ((media.groupspec.empty() != media.multicastStreamName.empty()) ||
                            UrlUtils::haveRtmfpScheme(media.url) == false) {
                        F4M_DLOG(std::cerr << __func__ << " multicast for rtmfp not valid " <<std::endl;);
                        continue;
                    }
                }

                if (m_manifestDoc->versionMajor() >= 3) {
                    // TODO : discard 'videoCodec' if 'type' is not ok

                    if (!media.drmAdditionalHeaderId.empty() && !media.drmAdditionalHeaderSetId.empty()) {
                        F4M_DLOG(std::cerr << __func__
                                 << " both drmAdditionalHeaderId and drmAdditionalHeaderSetId are present"
                                 <<std::endl;);
                        // TODO: ?
                    }
                }

                // push to manifest
                manifest->medias.push_back(media);

                // Only one if we're in a multi-level stream-level
                if (m_manifestDoc->isMultiLevelStreamLevel() == true) {
                    break;
                }
            }
        }
        xmlXPathFreeObject(xpathObj);
    }
}

void ManifestParser::parseBootstrapInfos(Manifest *manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:bootstrapInfo");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                BootstrapInfo bootstrapInfo;

                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
                    if (!attr->name) {
                        continue;
                    }
                    if (attrNameIs(attr, "profile")) {
                        bootstrapInfo.profile = getNodeAttributeValueAsString(attr);  //mandatory
                    } else if (attrNameIs(attr, "id")) {
                        bootstrapInfo.id = getNodeAttributeValueAsString(attr);
                    } else if (attrNameIs(attr, "url")) {
                        bootstrapInfo.url = getNodeAttributeValueAsString(attr);
                        if (UrlUtils::isAbsolute(bootstrapInfo.url) == false) {
                            bootstrapInfo.url.insert(0, manifest->baseURL + "/");
                        }
                    }

                    // TODO: separate by version cleanly
                    else if (m_manifestDoc->versionMajor() >= 3
                             && attrNameIs(attr, "fragmentDuration")) {
                        bootstrapInfo.fragmentDuration = getNodeAttributeValueAsFloat(attr);
                    } else if (m_manifestDoc->versionMajor() >= 3
                               && attrNameIs(attr, "segmentDuration")) {
                        bootstrapInfo.segmentDuration = getNodeAttributeValueAsFloat(attr);
                    }

                    else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attr " << (const char *)attr->name
                                 << " ignored" << std::endl;);
                        getNodeAttributeValueAsString(attr);  // D
                    }
                }

                // check
                if (bootstrapInfo.profile.empty()) {
                    F4M_DLOG(std::cerr << __func__ << " ignoring malformed bootstrap : no profile attr"
                             << std::endl;);
                    continue;
                }

                if (bootstrapInfo.url.empty()) {
                    bootstrapInfo.data = getNodeBase64Data(node);
                    // check
                    if (bootstrapInfo.data.empty()) {
                        F4M_DLOG(std::cerr << __func__ << " ignoring malformed bootstrap : no data"
                                 << std::endl;);
                        continue;
                    }
                }

                auto assign = [&](Media &media) {
                    if (media.bootstrapInfoId.empty() ||
                            media.bootstrapInfoId == bootstrapInfo.id) {
                        media.bootstrapInfo = bootstrapInfo;
                    }
                };
                forEachMedia(manifest, assign);
            }

        }
        xmlXPathFreeObject(xpathObj);
    }
}

void ManifestParser::parseSmpteTimeCodes(Manifest *manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:smpteTimecodes/nsf4m:smpteTimecode");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                SmpteTimecode smpteTimeCode;

                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {

                    if (attrNameIs(attr, "timestamp")) {
                        // 0.0 is valid, could be a problem here
                        smpteTimeCode.timestamp = getNodeAttributeValueAsFloat(attr);
                        continue;
                    } else if (attrNameIs(attr, "smpte")) {
                        smpteTimeCode.smpte = getNodeAttributeValueAsString(attr);
                        continue;
                    } if (attrNameIs(attr, "date")) {
                        smpteTimeCode.date = getNodeAttributeValueAsString(attr);
                        continue;
                    } if (attrNameIs(attr, "timezone")) {
                        smpteTimeCode.timezone = getNodeAttributeValueAsString(attr);
                        continue;
                    } else {
                        F4M_DLOG(std::cerr << __func__ << " ignoring attr"
                                 << (const char *)attr->name
                                 << std::endl;);
                    }
                }

                // ignore malformed
                if (smpteTimeCode.timestamp < 0.0 || smpteTimeCode.smpte.empty()) {
                    F4M_DLOG(std::cerr << __func__ << " ignoring malformed smpteTimeCode"
                             << std::endl;);
                    continue;
                }

                // TODO: check formats, check that timestamp is increasing

                // to manifest

                auto assign = [&](Media media) {
                    media.smpteTimeCodes.push_back(smpteTimeCode);
                };
                forEachMedia(manifest, assign);

            }
        }
        xmlXPathFreeObject(xpathObj);
    }
}

void ManifestParser::parseCueInfos(Manifest *manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:cueInfo");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                std::string id;

                // get the id
                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
                    if (attrNameIs(attr, "id")) {
                        id = getNodeAttributeValueAsString(attr);
                        continue;
                    } else {
                        F4M_DLOG(std::cerr << __func__ << " ignoring cueInfo attr "
                                 << (const char *)attr->name
                                 << std::endl;);
                    }
                }

                // ignore malformed
                if (id.empty()) {
                    F4M_DLOG(std::cerr << __func__ << " ignoring cueInfo withour id " << std::endl;);
                    continue;
                }

                std::vector<Cue> cues;

                // get the children Cue
                for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
                    // check we don't escape ns
                    if (child->type == XML_ELEMENT_NODE && nodeIsInF4mNs(child) && child->name) {

                        if (nodeNameIs(child, "cue")) {

                            Cue cue;
                            for (xmlAttrPtr attr = child->properties; attr != NULL; attr = attr->next) {

                                if (attrNameIs(attr, "availNum")) {
                                    cue.availNum = getNodeAttributeValueAsInt(attr);
                                    continue;
                                } else if (attrNameIs(attr, "availsExpected")) {
                                    cue.availsExpected = getNodeAttributeValueAsInt(attr);
                                    continue;
                                } else if (attrNameIs(attr, "duration")) {
                                    cue.duration = getNodeAttributeValueAsFloat(attr);
                                    continue;
                                } else if (attrNameIs(attr, "id")) {
                                    cue.id = getNodeAttributeValueAsString(attr);
                                    continue;
                                } else if (attrNameIs(attr, "time")) {
                                    cue.time = getNodeAttributeValueAsFloat(attr);
                                    continue;
                                } else if (attrNameIs(attr, "type")) {
                                    cue.type = getNodeAttributeValueAsString(attr);
                                    continue;
                                } else if (attrNameIs(attr, "programId")) {
                                    cue.programId = getNodeAttributeValueAsString(attr);
                                    continue;
                                }  else {
                                    F4M_DLOG(std::cerr << __func__ << " ignoring cue attr "
                                             << (const char *)attr->name << std::endl;);
                                }
                            }

                            // TODO: check. Cue must be stored in ascending time order
                            if (cue.duration < 0.0 || cue.id.empty() || cue.time < 0.0
                                    || cue.type.empty() || cue.type != "spliceOut") {
                                F4M_DLOG(std::cerr << __func__ << " ignoring malformed cue"
                                         << std::endl;);
                                continue;
                            }
                            cues.push_back(cue);
                        }
                    }
                }

                if (!cues.empty()) {
                    auto assign = [&](Media media) {
                        if (!media.cueInfoId.empty() && media.cueInfoId == id) {
                            media.cueInfo = cues;
                        }
                    };
                    forEachMedia(manifest, assign);
                } else {
                    F4M_DLOG(std::cerr << __func__ << " ignoring empty cueInfo"
                             << std::endl;);
                }

            }
        }
        xmlXPathFreeObject(xpathObj);
    }
}

// TODO: to consider only if not available from bootstrapInfo
void ManifestParser::parseBestEffortFetchInfo(Manifest *manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:bestEffortFetchInfo");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                BestEffortFetchInfo bestEffortFetchInfo;

                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
                    if (!attr->name) {
                        continue;
                    }

                    if (attrNameIs(attr, "id")) {
                        bestEffortFetchInfo.id = getNodeAttributeValueAsString(attr);//mandatory
                    }
                    else if (attrNameIs(attr, "fragmentDuration")) {
                        bestEffortFetchInfo.fragmentDuration = getNodeAttributeValueAsFloat(attr);
                    }
                    else if (attrNameIs(attr, "segmentDuration")) {
                        bestEffortFetchInfo.segmentDuration = getNodeAttributeValueAsFloat(attr);
                    }
                    else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attr " << (const char *)attr->name
                                 << " ignored" << std::endl;);
                        getNodeAttributeValueAsString(attr);  // D
                    }
                }

                auto assign = [&](Media media) {
                    if (media.bestEffortFetchInfoId.empty() ||
                            media.bestEffortFetchInfoId == bestEffortFetchInfo.id ||
                            bestEffortFetchInfo.id.empty()) {
                        media.bestEffortFetchInfo = bestEffortFetchInfo;
                    }
                };
                forEachMedia(manifest, assign);

            }
        }
        xmlXPathFreeObject(xpathObj);
    }
}

void ManifestParser::parseDrmAdditionalHeaderSet(Manifest *manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:drmAdditionalHeaderSet");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                // process each set : it means get the child and group them together
                std::string id;

                // get the id
                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
                    if (attrNameIs(attr, "id")) {
                        id = getNodeAttributeValueAsString(attr);
                    } else {
                        F4M_DLOG(std::cerr << __func__ << " ignoring drmAdditionalHeaderSet attr "
                                 << (const char *)attr->name << std::endl;);
                    }
                }

                std::vector<DrmAdditionalHeader> dAHs;

                for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
                    // check we don't escape ns
                    if (child->type == XML_ELEMENT_NODE && nodeIsInF4mNs(child) && child->name) {

                        if (nodeNameIs(child, "drmAdditionalHeader")) {

                            DrmAdditionalHeader drmAdditionalHeader;

                            for (xmlAttrPtr attr = child->properties; attr != NULL; attr = attr->next) {
                                if (!attr->name) {
                                    continue;
                                }
                                if (attrNameIs(attr, "id")) {
                                    drmAdditionalHeader.id = getNodeAttributeValueAsString(attr);
                                } else if (attrNameIs(attr, "drmContentId")) {
                                    drmAdditionalHeader.drmContentId = getNodeAttributeValueAsString(attr);
                                }
                                // only in Set
                                else if (attrNameIs(attr, "prefetchDeadline")) {
                                    drmAdditionalHeader.prefetchDeadline = getNodeAttributeValueAsFloat(attr);
                                }  else if (attrNameIs(attr, "startTimestamp")) {
                                    drmAdditionalHeader.startTimestamp = getNodeAttributeValueAsFloat(attr);
                                }
                                else if (attrNameIs(attr, "url")) {
                                    drmAdditionalHeader.url = getNodeAttributeValueAsString(attr);
                                    if (UrlUtils::isAbsolute(drmAdditionalHeader.url) == false) {
                                        drmAdditionalHeader.url.insert(0, manifest->baseURL + "/");
                                    }
                                } else {
                                    F4M_DLOG(std::cerr << __func__ <<  " : attr " << (const char *)attr->name
                                             << " ignored" << std::endl;);
                                    getNodeAttributeValueAsString(attr);  // D
                                }
                            }

                            // get the data if it's here
                            if (drmAdditionalHeader.url.empty()) {
                                drmAdditionalHeader.data = getNodeBase64Data(child);
                                // check
                                if (drmAdditionalHeader.data.empty()) {
                                    F4M_DLOG(std::cerr << __func__ << "ignoring malformed drmAdditionalHeader : no data"
                                             << std::endl;);
                                    continue;
                                }
                            }

                            // TODO: check
                            dAHs.push_back(drmAdditionalHeader);
                        }
                    }
                }

                auto assign = [&](Media media) {
                    if (id.empty() || media.drmAdditionalHeaderId == id) {
                        media.drmAdditionalHeaderSet = dAHs;
                    }
                };
                forEachMedia(manifest, assign);
            }
        }
        xmlXPathFreeObject(xpathObj);
    }
}

// for now just duplicate parseMedias code
// I'll see if I can get the whole manifest parsing accurate later
void ManifestParser::parseAdaptiveSet(Manifest *manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:adaptiveSet");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                // process each set : it means get the children and group them together
                bool alternate = false;
                std::string audioCodec;
                std::string label;
                std::string lang;
                std::string type;

                std::vector<Media> medias;

                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
                    if (attrNameIs(attr, "alternate")) {
                        alternate = true;
                        continue;
                    } else if (attrNameIs(attr, "label")) {
                        label = getNodeAttributeValueAsString(attr);
                        continue;
                    } else if (attrNameIs(attr, "lang")) {
                        lang = getNodeAttributeValueAsString(attr);
                        continue;
                    } else if (attrNameIs(attr, "type")) {
                        type = getNodeAttributeValueAsString(attr);
                        continue;
                    } {
                        F4M_DLOG(std::cerr << __func__ << " ignoring adaptiveSet attr "
                                 << (const char *)attr->name
                                 << std::endl;);
                    }
                }

                // then get the media nodes
                for (xmlNodePtr child = node->children; child != NULL; child = child->next) {
                    // check we don't escape ns
                    if (child->type == XML_ELEMENT_NODE && nodeIsInF4mNs(child) && child->name) {

                        if (nodeNameIs(child, "media") == false) {
                            F4M_DLOG(std::cerr << __func__ << " ignoring adaptiveSet child element "
                                     << (const char *)child->name
                                     << std::endl;);
                            continue;
                        }

                        Media media;

                        // set attrs
                        media.alternate = alternate;
                        media.label = label;
                        media.lang = lang;
                        media.audioCodec = audioCodec;
                        media.type = type;

                        // get attrs
                        for (xmlAttrPtr attr = child->properties; attr != NULL; attr = attr->next) {
                            if (!attr->name) {
                                continue;
                            }

                            if (attrNameIs(attr, "href")) {
                                media.href = getNodeAttributeValueAsString(attr);
                                if (UrlUtils::isAbsolute(media.href) == false) {
                                    media.href.insert(0, manifest->baseURL + "/");
                                }
                            } else if (attrNameIs(attr, "videoCodec")) {
                                media.videoCodec = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "cueInfoId")) {
                                media.cueInfoId = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "bestEffortFetchInfoId")) {
                                media.bestEffortFetchInfoId = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "drmAdditionalHeaderSetId")) {
                                media.drmAdditionalHeaderSetId = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "bitrate")) {
                                media.bitrate = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "streamId")) {
                                media.streamId = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "width")) {
                                media.width = getNodeAttributeValueAsInt(attr);
                            } else if (attrNameIs(attr, "height")) {
                                media.height = getNodeAttributeValueAsInt(attr);
                            } else if (attrNameIs(attr, "url")) {
                                media.url = getNodeAttributeValueAsString(attr);
                                if (UrlUtils::isAbsolute(media.url) == false) {
                                    media.url.insert(0, manifest->baseURL + "/");
                                }
                            } else if (attrNameIs(attr, "bootstrapInfoId")) {
                                media.bootstrapInfoId = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "drmAdditionalHeaderId")) {
                                media.drmAdditionalHeaderId = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "groupspec")) {
                                media.groupspec = getNodeAttributeValueAsString(attr);
                            } else if (attrNameIs(attr, "multicastStreamName")) {
                                media.multicastStreamName = getNodeAttributeValueAsString(attr);
                            } else {
                                F4M_DLOG(std::cerr << __func__ <<  " : attr "
                                         << (const char *)attr->name << " ignored" << std::endl;);
                                getNodeAttributeValueAsString(attr);
                            }
                        }

                        // get metadat if here
                        for (xmlNodePtr lchild = child->children; lchild != NULL; lchild = lchild->next) {
                            // check we don't escape ns
                            if (lchild->type == XML_ELEMENT_NODE && nodeIsInF4mNs(lchild) && lchild->name) {

                                if (nodeNameIs(lchild, "metadata")) {
                                    media.metadata = getNodeBase64Data(lchild);
                                }

                            }
                        }
                        medias.push_back(media);
                    }
                }

                // then push adaptiveSet to manifest
                AdaptiveSet aSet;
                aSet.alternate = alternate;
                aSet.label = label;
                aSet.lang = lang;
                aSet.audioCodec = audioCodec;
                aSet.type = type;
                aSet.medias = medias;
                manifest->adaptiveSets.push_back(aSet);
            }
        }
        xmlXPathFreeObject(xpathObj);
    }
}

void ManifestParser::parseDvrInfos(Manifest *manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:dvrInfo");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                DvrInfo dvrInfo;

                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {

                    if (!attr->name) {
                        continue;
                    }

                    if (m_manifestDoc->versionMajor() == 1) {
                        if (attrNameIs(attr, "id")) {
                            dvrInfo.id = getNodeAttributeValueAsString(attr);
                            continue;
                        } else if (attrNameIs(attr, "beginOffset")) {
                            dvrInfo.beginOffset = getNodeAttributeValueAsInt(attr);
                            continue;
                        } else if (attrNameIs(attr, "endOffset")) {
                            dvrInfo.endOffset = getNodeAttributeValueAsInt(attr);
                            continue;
                        }

                    } else if (m_manifestDoc->versionMajor() >= 2) {
                        if (attrNameIs(attr, "windowDuration")) {
                            dvrInfo.windowDuration = getNodeAttributeValueAsInt(attr);
                            continue;
                        }
                    }

                    if (attrNameIs(attr, "url")) {
                        dvrInfo.url = getNodeAttributeValueAsString(attr);
                        if (UrlUtils::isAbsolute(dvrInfo.url) == false) {
                            dvrInfo.url.insert(0, manifest->baseURL + "/");
                        }
                    } else if (attrNameIs(attr, "offline")) {
                        dvrInfo.offline = true;
                    } else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attr " <<(const char *)attr->name
                                 << " ignored" << std::endl;);
                        getNodeAttributeValueAsString(attr);  // D
                    }
                }

                auto assign = [&](Media media) {
                    if (m_manifestDoc->versionMajor() >= 2 ||
                            (media.dvrInfoId.empty() || media.dvrInfoId == dvrInfo.id)) {
                        media.dvrInfo = dvrInfo;
                    }
                };
                forEachMedia(manifest, assign);

            }
        }
        xmlXPathFreeObject(xpathObj);
    }
}

void ManifestParser::parseDrmAdditionalHeaders(Manifest *manifest)
{
    const xmlChar *xpathExp = BAD_CAST("/nsf4m:manifest/nsf4m:drmAdditionalHeader");
    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(xpathExp, m_manifestDoc->xpathCtx());

    if (xpathObj) {

        if (xpathObj->nodesetval) {

            for (auto i = 0; i < xpathObj->nodesetval->nodeNr; ++i) {

                const xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                if (!node || node->type != XML_ELEMENT_NODE) {
                    continue;
                }

                DrmAdditionalHeader drmAdditionalHeader;

                for (xmlAttrPtr attr = node->properties; attr != NULL; attr = attr->next) {
                    if (!attr->name) {
                        continue;
                    }
                    if (attrNameIs(attr, "id")) {
                        drmAdditionalHeader.id = getNodeAttributeValueAsString(attr);
                    } else if (attrNameIs(attr, "drmContentId")) {
                        drmAdditionalHeader.drmContentId = getNodeAttributeValueAsString(attr);
                    } else if (attrNameIs(attr, "url")) {
                        drmAdditionalHeader.url = getNodeAttributeValueAsString(attr);
                        if (UrlUtils::isAbsolute(drmAdditionalHeader.url) == false) {
                            drmAdditionalHeader.url.insert(0, manifest->baseURL + "/");
                        }
                    } else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attr " << (const char *)attr->name
                                 << " ignored" << std::endl;);
                        getNodeAttributeValueAsString(attr);  // D
                    }
                }

                if (drmAdditionalHeader.url.empty()) {
                    drmAdditionalHeader.data = getNodeBase64Data(node);
                    // check
                    if (drmAdditionalHeader.data.empty()) {
                        F4M_DLOG(std::cerr << __func__ << "ignoring malformed drmAdditionalHeader : no data"
                                 << std::endl;);
                        continue;
                    }
                }

                auto assign = [&](Media media) {
                    if (media.drmAdditionalHeaderId.empty() ||
                            media.drmAdditionalHeaderId == drmAdditionalHeader.id) {
                        media.drmAdditionalHeader = drmAdditionalHeader;
                    }
                };
                forEachMedia(manifest, assign);
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
    auto getAttributeValueAsString = [&](const xmlAttr* attr) {
        std::string str;
        if (attr && attr->children) {
            xmlChar* attrStr = xmlNodeListGetString(doc, attr->children, 1);
            if (attrStr) {
                str = (const char *)attrStr;
                F4M_DLOG(std::cerr << __func__  << " [" << attr->name << "] = " + str << std::endl;);
                xmlFree(attrStr);
            }
        }
        return str;
    };

    auto getAttributeValueAsInt = [&](const xmlAttr* attr) {
        int res = 0;
        std::string str = getAttributeValueAsString(attr);
        if (str.empty()) {
            return res;
        }
        try { res = std::stoi(str); } catch (...) {
            F4M_DLOG(std::cerr << __func__ <<  " [" << attr->name << "] " << "not an int "
                     << res << std::endl;);
            res = 0; } // !
        return res;
    };

    // check we can download
    if (!downloadFileUserPtr || url.empty() || !UrlUtils::haveHttpScheme(url)) {
        F4M_DLOG(std::cerr << __func__ << " unable to download from url" << std::endl;);
        goto fail;
    }

    // get xml file : I'm not sure for authority, set it to false
    response = downloadFileFctPtr(downloadFileUserPtr, url, status );
    if (status != 200 || response.empty()) {
        response.clear();
        F4M_DLOG(std::cerr << __func__ << " get dvrInfo failed with status " << status << std::endl;);
        goto fail;
    }
    response.push_back('\0');

    // get xml doc
    // response not empty : &response[0] defined
    doc = xmlReadMemory(reinterpret_cast<const char *>(&response[0]),
                        (int)response.size(), "manifest.f4m", NULL, 0);
    if (!doc) {
        F4M_DLOG(std::cerr << __func__ << " xmlReadMemory failed" << std::endl;);
        goto fail;
    }

    // get root
    root = xmlDocGetRootElement(doc);
    if (!root) {
        F4M_DLOG(std::cerr << __func__ << " xmlDocGetRootElement failed" << std::endl;);
        goto fail;
    }

    // check tag
    if (strcmp((const char *)root->name, "dvrInfo")) {
        F4M_DLOG(std::cerr << __func__ << " root tag is not dvrInfo failed" << std::endl;);
        goto fail;
    }

    // get attrs minus url
    for (xmlAttrPtr attr = root->properties; attr != NULL; attr = attr->next) {
        if (!attr->name) {
            continue;
        }
        if (attrNameIs(attr, "id")) {
            dvrInfo->id = getAttributeValueAsString(attr);
        } else if (attrNameIs(attr, "beginOffset")) {
            dvrInfo->beginOffset = getAttributeValueAsInt(attr);
        } else if (attrNameIs(attr, "endOffset")) {
            dvrInfo->endOffset = getAttributeValueAsInt(attr);
        }else if (attrNameIs(attr, "windowDuration")) {
            dvrInfo->windowDuration = getAttributeValueAsInt(attr);
        } else if (attrNameIs(attr, "offline")) {
            dvrInfo->offline = true;
        } else {
            F4M_DLOG(std::cerr << __func__ <<  " : attr " <<(const char *)attr->name
                     << " ignored" << std::endl;);
            getAttributeValueAsString(attr);  // D
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
