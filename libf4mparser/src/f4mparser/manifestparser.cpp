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

#include "urlutils.h"

#include <cstring>
#include <pugixml.hpp>

#ifndef F4M_DEBUG
#define F4M_DLOG(x)
#else
#include <iostream> // std::cerr
#define F4M_DLOG(x) do { x } while(0)
#endif

ManifestParser::ManifestParser(void *downloadFileUserPtr,
                               ManifestParser::DOWNLOAD_FILE_FUNCTION downloadFileFctPtr)
    : m_downloadFileUserPtr(downloadFileUserPtr), m_downloadFileFctPtr(downloadFileFctPtr)
{
}

bool ManifestParser::parse(std::string url, Manifest *manifest)
{
    m_f4mDoc = std::unique_ptr<ManifestDoc>(new ManifestDoc{url});

    if (!initManifestParser()) {
        return false;
    }

    setManifestLevel();

    *manifest = parseManifest();

    if (m_f4mDoc->isSetLevel()) {
        parseMLStreamManifests(manifest);
    }

    return true;
}

bool ManifestParser::initManifestParser()
{
    if (m_f4mDoc->fileUrl().empty()) {
        F4M_DLOG(std::cerr << __func__ << " manifest url empty" << std::endl;);
        return false;
    }

    if (UrlUtils::haveHttpScheme(m_f4mDoc->fileUrl()) == false) {
        F4M_DLOG(std::cerr << __func__
                 << " manifest url scheme don't begin with http" << std::endl;);
        return false;
    }

    std::vector<uint8_t> response;
    if (downloadF4mFile(&response) == false) {
        F4M_DLOG(std::cout << __func__ << " failed to dowload manifest" << std::endl;);
        return false;
    }

    if (m_f4mDoc->setXmlDoc(std::move(response)) == false) {
        F4M_DLOG(std::cerr << __func__ << " setXmlDoc failed" << std::endl;);
        return false;
    }

    setManifestVersion(m_f4mDoc->rootNs());

    return true;
}

Manifest ManifestParser::parseManifest()
{
    Manifest manifest;
    if (m_f4mDoc->versionMajor() >= 2) {
        getManifestProfiles(&manifest);
    }

    std::string query{"/manifest" + m_f4mDoc->selectNs() + "/*" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {
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
            manifest.duration = getNodeContentAsNumber(node);
        }
        // report element we don't parse
        else if ((nodeNameIs(node, "media")
                  || nodeNameIs(node, "bootstrapInfo")
                  || nodeNameIs(node, "dvrInfo")
                  || nodeNameIs(node, "drmAdditionalHeader")
                  || nodeNameIs(node, "smpteTimecodes")
                  || nodeNameIs(node, "cueInfo")
                  || nodeNameIs(node, "bestEffortFetchInfo")
                  || nodeNameIs(node, "drmAdditionalHeaderSet")
                  || nodeNameIs(node, "adaptiveSet")) == false) {
            F4M_DLOG(std::cerr << __func__ <<  " : node : ["
                     << node.node().name() << "] ignored" << std::endl;);
            getNodeContentAsString(node);
        }
    }

    if (!manifest.deliveryType.empty() && manifest.deliveryType != "streaming"
            && manifest.deliveryType != "progressive") {
        F4M_DLOG(std::cerr << __func__ <<  "deliveryType invalid "
                 << manifest.deliveryType << std::endl;);
        manifest.deliveryType.clear();  // 'streaming' is the one used in practice
    }

    if (!manifest.streamType.empty() && manifest.streamType != "live"
            && manifest.streamType != "recorded"
            && manifest.streamType != "liveOrRecorded") {
        F4M_DLOG(std::cerr << __func__ <<  "streamType invalid "
                 << manifest.streamType << std::endl;);
        manifest.streamType.clear();  // or fallback to default "liveOrRecorded"
    }

    if (manifest.baseURL.empty()) {
        manifest.baseURL = sanitizeBaseUrl(m_f4mDoc->fileUrl());
    }

    parseMedias(&manifest);

    if (m_f4mDoc->versionMajor() >= 3
            && m_f4mDoc->isMultiLevelStreamLevel() == false) {
        parseAdaptiveSets(&manifest);
    }

    if (m_f4mDoc->isMultiLevelStreamLevel() == false) {
        parseDvrInfos(&manifest);
    }

    if (m_f4mDoc->isSetLevel() == false) {
        parseDrmAdditionalHeaders(&manifest);
        parseBootstrapInfos(&manifest);
    }

    if (m_f4mDoc->versionMajor() >= 3 ) {
        if (m_f4mDoc->isSetLevel() == false) {
            parseSmpteTimeCodes(&manifest);
            parseCueInfos(&manifest);
            parseDrmAdditionalHeaderSets(&manifest);
        }
        if (m_f4mDoc->isSetLevel() == true) {
            parseBestEffortFetchInfos(&manifest);
        }
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
    m_f4mDoc.reset(new ManifestDoc{media.href});

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
    subManifest.medias.at(0).dvrInfo = media.dvrInfo;

    // replace media
    media = subManifest.medias.at(0);

    // push profiles to main manifest
    for (auto profile : subManifest.profiles) {
        manifest->profiles.push_back(profile);
    }
}

void ManifestParser::parseMedias(Manifest* manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs() + "/media" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {

        Media media;

        for (auto &attr : node.node().attributes())
        {
            if (m_f4mDoc->versionMajor() == 1) {
                if (attrNameIs(attr, "dvrInfoId")) {
                    media.dvrInfoId = getAttrValueAsString(attr);
                    continue;
                }

            } else if (m_f4mDoc->versionMajor() >= 2) {
                if (attrNameIs(attr, "href")) {
                    media.href = getAttrValueAsString(attr);
                    if (UrlUtils::isAbsolute(media.href) == false) {
                        media.href.insert(0, manifest->baseURL + "/");
                    }
                    continue;
                }

                if (m_f4mDoc->versionMajor() >= 3) {
                    if (attrNameIs(attr, "audioCodec")) {
                        media.audioCodec = getAttrValueAsString(attr);
                        continue;
                    } else if (attrNameIs(attr, "videoCodec")) {
                        media.videoCodec = getAttrValueAsString(attr);
                        continue;
                    }
                    else if (attrNameIs(attr, "cueInfoId")) {
                        media.cueInfoId = getAttrValueAsString(attr);
                        continue;
                    }
                    else if (attrNameIs(attr, "bestEffortFetchInfoId")) {
                        media.bestEffortFetchInfoId = getAttrValueAsString(attr);
                        continue;
                    }
                    else if (attrNameIs(attr, "drmAdditionalHeaderSetId")) {
                        media.drmAdditionalHeaderSetId = getAttrValueAsString(attr);
                        continue;
                    }
                }
            }

            if (m_f4mDoc->isMultiLevelStreamLevel() == false) {

                if (attrNameIs(attr, "bitrate")) {
                    media.bitrate = getAttrValueAsString(attr);
                    continue;
                } else if (attrNameIs(attr, "streamId")) {
                    media.streamId = getAttrValueAsString(attr);
                    continue;
                } else if (attrNameIs(attr, "width")) {
                    media.width = getAttrValueAsInt(attr);
                    continue;
                } else if (attrNameIs(attr, "height")) {
                    media.height = getAttrValueAsInt(attr);
                    continue;
                } else if (attrNameIs(attr, "type")) {
                    media.type = getAttrValueAsString(attr);
                    continue;
                } else if (attrNameIs(attr, "alternate")) {
                    media.alternate = true;
                    continue;
                } else if (attrNameIs(attr, "label")) {
                    media.label = getAttrValueAsString(attr);
                    continue;
                } else if (attrNameIs(attr, "lang")) {
                    media.lang = getAttrValueAsString(attr);
                    continue;
                }
            }

            if (attrNameIs(attr, "url")) {
                media.url = getAttrValueAsString(attr);
                if (UrlUtils::isAbsolute(media.url) == false) {
                    media.url.insert(0, manifest->baseURL + "/");
                }
            } else if (attrNameIs(attr, "bootstrapInfoId")) {
                media.bootstrapInfoId = getAttrValueAsString(attr);
            } else if (attrNameIs(attr, "drmAdditionalHeaderId")) {
                media.drmAdditionalHeaderId = getAttrValueAsString(attr);
            } else if (attrNameIs(attr, "groupspec")) {
                media.groupspec = getAttrValueAsString(attr);
            } else if (attrNameIs(attr, "multicastStreamName")) {
                media.multicastStreamName = getAttrValueAsString(attr);
            }   else {
                F4M_DLOG(std::cerr << __func__ <<  " : attr "
                         << attr.name() << " ignored" << std::endl;);
                getAttrValueAsString(attr);
            }
        }

        for (auto &child : node.node().children()) {
            if (nodeIsInF4mNs(child)) {
                if (m_f4mDoc->versionMajor() == 1) {
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

        // check rtmfp
        if (!media.groupspec.empty() || !media.multicastStreamName.empty()) {
            if ((media.groupspec.empty() != media.multicastStreamName.empty()) ||
                    UrlUtils::haveRtmfpScheme(media.url) == false) {
                F4M_DLOG(std::cerr << __func__ << " multicast for rtmfp not valid " << std::endl;);
                continue;
            }
        }

        // check media type
        if (!media.type.empty()
                && media.type != "audio"
                && media.type != "audio+video"
                && media.type != "data"
                && media.type != "text"
                && media.type != "video"
                && !(m_f4mDoc->versionMajor() >= 3
                     && media.type == "video-keyframe-only")) {
            F4M_DLOG(std::cerr << __func__ << " invalid media type "
                     << media.type << std::endl;);
            media.type.clear(); // default to "audio+video"
        }

        // malformed manifest
        if (m_f4mDoc->versionMajor() >= 3) {
            if (!media.videoCodec.empty() && (!media.type.empty()  && media.type != "video"
                                              && media.type != "audio+video"
                                              && media.type != "video-keyframe-only")) {
                F4M_DLOG(std::cerr << __func__ << " videoCodec " << media.videoCodec
                         << " present with media type " << media.type
                         << std::endl;);
                media.videoCodec.clear();
            }

            if (!media.drmAdditionalHeaderId.empty() && !media.drmAdditionalHeaderSetId.empty()) {
                F4M_DLOG(std::cerr << __func__
                         << " both drmAdditionalHeaderId and drmAdditionalHeaderSetId are present"
                         <<std::endl;);
            }
        }

        printDebugMediaCheck(media);  // INFO

        // push to manifest
        manifest->medias.push_back(media);

        // Only one if we're in a multi-level stream-level
        if (m_f4mDoc->isMultiLevelStreamLevel()) {
            break;
        }
    }
}

// for now just duplicate parseMedias code
void ManifestParser::parseAdaptiveSets(Manifest *manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs() + "/adaptiveSet" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {

        bool alternate = false;
        std::string audioCodec;
        std::string label;
        std::string lang;
        std::string type;

        std::vector<Media> medias;

        for (auto &attr : node.node().attributes()) {

            if (attrNameIs(attr, "alternate")) {
                alternate = true;
                continue;
            } else if (attrNameIs(attr, "label")) {
                label = getAttrValueAsString(attr);
                continue;
            } else if (attrNameIs(attr, "lang")) {
                lang = getAttrValueAsString(attr);
                continue;
            } else if (attrNameIs(attr, "type")) {
                type = getAttrValueAsString(attr);
                continue;
            } {
                F4M_DLOG(std::cerr << __func__ << " ignoring adaptiveSet attr "
                         << attr.name() << std::endl;);
            }
        }

        // then get the media nodes
        for (auto &child : node.node().children()) {
            // check we don't escape ns
            if (nodeIsInF4mNs(child)) {

                if (nodeNameIs(child, "media") == false) {
                    F4M_DLOG(std::cerr << __func__ << " ignoring adaptiveSet child element "
                             << child.name() << std::endl;);
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
                for (auto &attr : child.attributes()) {

                    if (attrNameIs(attr, "href")) {
                        media.href = getAttrValueAsString(attr);
                        if (UrlUtils::isAbsolute(media.href) == false) {
                            media.href.insert(0, manifest->baseURL + "/");
                        }
                    } else if (attrNameIs(attr, "videoCodec")) {
                        media.videoCodec = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "cueInfoId")) {
                        media.cueInfoId = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "bestEffortFetchInfoId")) {
                        media.bestEffortFetchInfoId = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "drmAdditionalHeaderSetId")) {
                        media.drmAdditionalHeaderSetId = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "bitrate")) {
                        media.bitrate = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "streamId")) {
                        media.streamId = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "width")) {
                        media.width = getAttrValueAsInt(attr);
                    } else if (attrNameIs(attr, "height")) {
                        media.height = getAttrValueAsInt(attr);
                    } else if (attrNameIs(attr, "url")) {
                        media.url = getAttrValueAsString(attr);
                        if (UrlUtils::isAbsolute(media.url) == false) {
                            media.url.insert(0, manifest->baseURL + "/");
                        }
                    } else if (attrNameIs(attr, "bootstrapInfoId")) {
                        media.bootstrapInfoId = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "drmAdditionalHeaderId")) {
                        media.drmAdditionalHeaderId = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "groupspec")) {
                        media.groupspec = getAttrValueAsString(attr);
                    } else if (attrNameIs(attr, "multicastStreamName")) {
                        media.multicastStreamName = getAttrValueAsString(attr);
                    } else {
                        F4M_DLOG(std::cerr << __func__ <<  " : attr "
                                 << attr.name() << " ignored" << std::endl;);
                        getAttrValueAsString(attr);
                    }
                }

                // get metadata
                for (auto &lchild : child.children()) {
                    // check we don't escape ns
                    if (nodeIsInF4mNs(lchild)) {
                        if (nodeNameIs(lchild, "metadata")) {
                            media.metadata = getNodeBase64Data(lchild);
                        }
                    }
                }
                medias.push_back(media);
            }
        }

        // push adaptiveSet to manifest
        AdaptiveSet aSet;
        aSet.medias = medias;
        manifest->adaptiveSets.push_back(aSet);
    }

}

void ManifestParser::parseDvrInfos(Manifest *manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs() + "/dvrInfo" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {

        DvrInfo dvrInfo;

        for (auto &attr : node.node().attributes()) {

            if (m_f4mDoc->versionMajor() == 1) {
                if (attrNameIs(attr, "id")) {
                    dvrInfo.id = getAttrValueAsString(attr);
                    continue;
                } else if (attrNameIs(attr, "beginOffset")) {
                    dvrInfo.beginOffset = getAttrValueAsInt(attr);
                    continue;
                } else if (attrNameIs(attr, "endOffset")) {
                    dvrInfo.endOffset = getAttrValueAsInt(attr);
                    continue;
                }

            } else if (m_f4mDoc->versionMajor() >= 2) {
                if (attrNameIs(attr, "windowDuration")) {
                    dvrInfo.windowDuration = getAttrValueAsInt(attr);
                    continue;
                }
            }

            if (attrNameIs(attr, "url")) {
                dvrInfo.url = getAttrValueAsString(attr);
                if (UrlUtils::isAbsolute(dvrInfo.url) == false) {
                    dvrInfo.url.insert(0, manifest->baseURL + "/");
                }
            } else if (attrNameIs(attr, "offline")) {
                dvrInfo.offline = true;
            } else {
                F4M_DLOG(std::cerr << __func__ <<  " : attr " << attr.name()
                         << " ignored" << std::endl;);
                getAttrValueAsString(attr);  // D
            }
        }

        auto assign = [&](Media media) {
            if (m_f4mDoc->versionMajor() >= 2 ||
                    (media.dvrInfoId.empty() || media.dvrInfoId == dvrInfo.id)) {
                media.dvrInfo = dvrInfo;
            }
        };
        forEachMedia(manifest, assign);

    }
}

void ManifestParser::parseDrmAdditionalHeaders(Manifest *manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs()
                + "/drmAdditionalHeader" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {

        DrmAdditionalHeader drmAdditionalHeader;

        for (auto &attr : node.node().attributes()) {

            if (attrNameIs(attr, "id")) {
                drmAdditionalHeader.id = getAttrValueAsString(attr);
            }  else if (attrNameIs(attr, "url")) {
                drmAdditionalHeader.url = getAttrValueAsString(attr);
                if (UrlUtils::isAbsolute(drmAdditionalHeader.url) == false) {
                    drmAdditionalHeader.url.insert(0, manifest->baseURL + "/");
                }
            } else {
                F4M_DLOG(std::cerr << __func__ <<  " : attr " << attr.name()
                         << " ignored" << std::endl;);
                getAttrValueAsString(attr);  // D
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

void ManifestParser::parseBootstrapInfos(Manifest *manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs() + "/bootstrapInfo" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {

        BootstrapInfo bootstrapInfo;

        for (auto &attr : node.node().attributes()) {

            if (attrNameIs(attr, "profile")) {
                bootstrapInfo.profile = getAttrValueAsString(attr);  //mandatory
            } else if (attrNameIs(attr, "id")) {
                bootstrapInfo.id = getAttrValueAsString(attr);
            } else if (attrNameIs(attr, "url")) {
                bootstrapInfo.url = getAttrValueAsString(attr);
                if (UrlUtils::isAbsolute(bootstrapInfo.url) == false) {
                    bootstrapInfo.url.insert(0, manifest->baseURL + "/");
                }
            }

            else if (m_f4mDoc->versionMajor() >= 3
                     && attrNameIs(attr, "fragmentDuration")) {
                bootstrapInfo.fragmentDuration = getAttrValueAsNumber(attr);
            } else if (m_f4mDoc->versionMajor() >= 3
                       && attrNameIs(attr, "segmentDuration")) {
                bootstrapInfo.segmentDuration = getAttrValueAsNumber(attr);
            }

            else {
                F4M_DLOG(std::cerr << __func__ <<  " : attr " << attr.name()
                         << " ignored" << std::endl;);
                getAttrValueAsString(attr);  // D
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

void ManifestParser::parseSmpteTimeCodes(Manifest *manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs()
                + "/smpteTimecodes" + m_f4mDoc->selectNs()
                + "/smpteTimecode" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {

        SmpteTimecode smpteTimeCode;

        for (auto &attr : node.node().attributes()) {

            if (attrNameIs(attr, "timestamp")) {
                // 0.0 is valid, could be a problem here
                smpteTimeCode.timestamp = getAttrValueAsNumber(attr);
                continue;
            } else if (attrNameIs(attr, "smpte")) {
                smpteTimeCode.smpte = getAttrValueAsString(attr);
                continue;
            } if (attrNameIs(attr, "date")) {
                smpteTimeCode.date = getAttrValueAsString(attr);
                continue;
            } if (attrNameIs(attr, "timezone")) {
                smpteTimeCode.timezone = getAttrValueAsString(attr);
                continue;
            } else {
                F4M_DLOG(std::cerr << __func__ << " ignoring attr"
                         << attr.name()
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

        auto assign = [&](Media media) {
            media.smpteTimeCodes.push_back(smpteTimeCode);
        };
        forEachMedia(manifest, assign);

    }

}

void ManifestParser::parseCueInfos(Manifest *manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs() + "/cueInfo" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {

        std::string id;

        // get the id
        for (auto &attr : node.node().attributes()) {
            if (attrNameIs(attr, "id")) {
                id = getAttrValueAsString(attr);
                continue;
            } else {
                F4M_DLOG(std::cerr << __func__ << " ignoring cueInfo attr "
                         << attr.name()
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
        for (auto &child : node.node().children()) {

            // check we don't escape ns
            if (nodeIsInF4mNs(child)) {

                if (nodeNameIs(child, "cue")) {

                    Cue cue;
                    for (auto &attr : child.attributes()) {

                        if (attrNameIs(attr, "availNum")) {
                            cue.availNum = getAttrValueAsInt(attr);
                            continue;
                        } else if (attrNameIs(attr, "availsExpected")) {
                            cue.availsExpected = getAttrValueAsInt(attr);
                            continue;
                        } else if (attrNameIs(attr, "duration")) {
                            cue.duration = getAttrValueAsNumber(attr);
                            continue;
                        } else if (attrNameIs(attr, "id")) {
                            cue.id = getAttrValueAsString(attr);
                            continue;
                        } else if (attrNameIs(attr, "time")) {
                            cue.time = getAttrValueAsNumber(attr);
                            continue;
                        } else if (attrNameIs(attr, "type")) {
                            cue.type = getAttrValueAsString(attr);
                            continue;
                        } else if (attrNameIs(attr, "programId")) {
                            cue.programId = getAttrValueAsString(attr);
                            continue;
                        }  else {
                            F4M_DLOG(std::cerr << __func__ << " ignoring cue attr "
                                     << attr.name() << std::endl;);
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
            F4M_DLOG(std::cerr << __func__ << " ignoring empty cueInfo" << std::endl;);
        }

    }

}

void ManifestParser::parseBestEffortFetchInfos(Manifest *manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs()
                + "/bestEffortFetchInfo" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());


    for (auto &node : nodes) {

        BestEffortFetchInfo bestEffortFetchInfo;

        for (auto &attr : node.node().attributes()) {

            if (attrNameIs(attr, "id")) {
                bestEffortFetchInfo.id = getAttrValueAsString(attr);//mandatory
            }
            else if (attrNameIs(attr, "fragmentDuration")) {
                bestEffortFetchInfo.fragmentDuration = getAttrValueAsNumber(attr);
            }
            else if (attrNameIs(attr, "segmentDuration")) {
                bestEffortFetchInfo.segmentDuration = getAttrValueAsNumber(attr);
            }
            else {
                F4M_DLOG(std::cerr << __func__ <<  " : attr " << attr.name()
                         << " ignored" << std::endl;);
                getAttrValueAsString(attr);  // D
            }
        }

        // info
        if (nodes.size() > 1 && bestEffortFetchInfo.id.empty()) {
            F4M_DLOG(std::cerr << __func__
                     <<  " several bestEffortFetchInfo but no id" << std::endl;);
        }

        auto assign = [&](Media media) {
            if (media.bestEffortFetchInfoId.empty() ||
                    media.bestEffortFetchInfoId == bestEffortFetchInfo.id ||
                    bestEffortFetchInfo.id.empty()) {
                // consider only if not set in bootstrapInfo
                if (media.bootstrapInfo.fragmentDuration < 0
                        &&  media.bootstrapInfo.segmentDuration < 0) {
                    media.bestEffortFetchInfo = bestEffortFetchInfo;
                }
            }
        };
        forEachMedia(manifest, assign);

    }

}

void ManifestParser::parseDrmAdditionalHeaderSets(Manifest *manifest)
{
    std::string query{"/manifest" + m_f4mDoc->selectNs()
                + "/drmAdditionalHeaderSet" + m_f4mDoc->selectNs()};

    pugi::xpath_node_set nodes = m_f4mDoc->doc().select_nodes(query.data());

    for (auto &node : nodes) {

        std::string id;

        // get the id
        for (auto &attr : node.node().attributes()) {
            if (attrNameIs(attr, "id")) {
                id = getAttrValueAsString(attr);
            } else {
                F4M_DLOG(std::cerr << __func__ << " ignoring drmAdditionalHeaderSet attr "
                         << attr.name() << std::endl;);
            }
        }

        std::vector<DrmAdditionalHeader> dAHs;

        for (auto &child : node.node().children()) {

            // check we don't escape ns
            if (nodeIsInF4mNs(child)) {

                if (nodeNameIs(child, "drmAdditionalHeader")) {

                    DrmAdditionalHeader drmAdditionalHeader;

                    for (auto &attr : child.attributes()) {

                        if (attrNameIs(attr, "id")) {
                            drmAdditionalHeader.id = getAttrValueAsString(attr);
                        }
                        // only in Set
                        else if (attrNameIs(attr, "prefetchDeadline")) {
                            drmAdditionalHeader.prefetchDeadline = getAttrValueAsNumber(attr);
                        }  else if (attrNameIs(attr, "startTimestamp")) {
                            drmAdditionalHeader.startTimestamp = getAttrValueAsNumber(attr);
                        }
                        else if (attrNameIs(attr, "url")) {
                            drmAdditionalHeader.url = getAttrValueAsString(attr);
                            if (UrlUtils::isAbsolute(drmAdditionalHeader.url) == false) {
                                drmAdditionalHeader.url.insert(0, manifest->baseURL + "/");
                            }
                        } else {
                            F4M_DLOG(std::cerr << __func__ <<  " : attr " << attr.name()
                                     << " ignored" << std::endl;);
                            getAttrValueAsString(attr);  // D
                        }
                    }

                    // get the data if it's here
                    if (drmAdditionalHeader.url.empty()) {
                        drmAdditionalHeader.data = getNodeBase64Data(child);
                        // check
                        if (drmAdditionalHeader.data.empty()) {
                            F4M_DLOG(std::cerr << __func__
                                     << "ignoring malformed drmAdditionalHeader : no data"
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

bool ManifestParser::updateDvrInfo(void *downloadFileUserPtr,
                                   DOWNLOAD_FILE_FUNCTION downloadFileFctPtr,
                                   const std::string &url,
                                   DvrInfo *dvrInfo)
{
    long status = -1;
    std::vector<uint8_t> response;
    pugi::xml_document doc;

    // check we can download
    if (!downloadFileUserPtr || url.empty() || !UrlUtils::haveHttpScheme(url)) {
        F4M_DLOG(std::cerr << __func__ << " unable to download from url" << std::endl;);
        return false;
    }

    // get xml file
    response = downloadFileFctPtr(downloadFileUserPtr, url, status);
    if (status != 200 || response.empty()) {
        response.clear();
        F4M_DLOG(std::cerr << __func__
                 << " get dvrInfo failed with status " << status << std::endl;);
        return false;
    }
    response.push_back('\0');

    // get xml doc
    // response not empty
    pugi::xml_parse_result result = doc.load_buffer_inplace(response.data(), response.size());
    if (!result) {
        F4M_DLOG(std::cerr << __func__ << " Error description: " << result.description() << "\n";);
        return false;
    }

    // check tag
    if (strcmp(doc.first_child().name(), "dvrInfo")) {
        F4M_DLOG(std::cerr << __func__ << " root tag is not dvrInfo" << std::endl;);
        return false;
    }

    // get attrs minus url
    for (auto &attr : doc.first_child().attributes()) {

        if (attrNameIs(attr, "id")) {
            dvrInfo->id = getAttrValueAsString(attr);
        } else if (attrNameIs(attr, "beginOffset")) {
            dvrInfo->beginOffset = getAttrValueAsInt(attr);
        } else if (attrNameIs(attr, "endOffset")) {
            dvrInfo->endOffset = getAttrValueAsInt(attr);
        }else if (attrNameIs(attr, "windowDuration")) {
            dvrInfo->windowDuration = getAttrValueAsInt(attr);
        } else if (attrNameIs(attr, "offline")) {
            dvrInfo->offline = true;
        } else {
            F4M_DLOG(std::cerr << __func__ <<  " : attr " << attr.name()
                     << " ignored" << std::endl;);
            getAttrValueAsString(attr);  // D
        }
    }

    return true;
}
