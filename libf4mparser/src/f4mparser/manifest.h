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

/*! \file manifest.h
 *  \brief Structures for holding datas from manifest.
 *
 *  This contains the medias extracted from parsing the manifest.
 *
 *  Information principally gathered from :
 *  http://sourceforge.net/apps/mediawiki/osmf.adobe/index.php?title=Flash_Media_Manifest_%28F4M%29_File_Format_obsolete
 *
 *  \author (rafirafi)
 */

#ifndef MANIFEST_H
#define MANIFEST_H

#include <string>
#include <vector>
#include <cstdint> // uint8_t

/*! \brief The <drmAdditionalHeader> element represents the DRM AdditionalHeader
 * needed for DRM authentication. It contains either a BASE64 encoded
 * representation of, or a URL to, the DRM AdditionalHeader (including
 * the serialized "|AdditionalHeader" string). It is optional.
 *
 * raw DRM AdditionalHeader is extracted from inlined base64.
 * The <drmAdditionalHeader> element's scope is the file it resides in.
 * For MLM(multi-level manifest), it's valid only in the stream-level manifest.
 */
class DrmAdditionalHeader
{
public:
    DrmAdditionalHeader() : empty{false} {}
    bool empty; ///< If true it's the default structure, else it's the result of parsing

    std::string id; ///< The ID of this <drmAdditionalHeader> element. It is optional
    std::string url; ///< A URL to a file containing the raw DRM AdditionalHeader.
        /// Either the url attribute or the inline BASE64 header (but not both) must be specified.
    std::vector<uint8_t> data; ///< The raw DRM AdditionalHeader
    std::string drmContentId; ///< Undocumented
};

/*! \brief The <dvrInfo> element represents all information needed to play DVR
 * media. It contains no content, only attributes. It is optional.
 *
 * For MLM(multi-level manifest), DvrInfo in stream-level manifest should be ignored.
 */
class DvrInfo
{
public:
    DvrInfo() : empty{false}, beginOffset{0}, endOffset{0}, offline{false}, windowDuration{-1} {}
    bool empty; ///< If true it's the default structure, else it's the result of parsing

    std::string id; ///< The ID of this <dvrInfo> element. It is optional.  F4M 1.0 only
    int beginOffset; ///< The offset, in seconds, from the beginning of the recorded stream. It is optional.  F4M 1.0 only
    int endOffset; ///< The amount of data, in seconds, that clients can view behind the current duration. It is optional.  F4M 1.0 only
    bool offline; ///< Indicates whether the stream is offline, or available for playback. It is optional, and defaults to false.
    std::string url; ///< A URL to a file containing the DVR info.
    int windowDuration; ///< The amount of data, in seconds, that clients can view behind the live point. F4M 2.0 only
};

/*! \brief The <bootstrapInfo> element represents all information needed to
 * bootstrap playback of HTTP streamed media.  It contains either a BASE64
 * encoded representation of, or a URL to, the bootstrap information
 * in the format that corresponds to the bootstrap profile. It is optional.
 *
 *  Raw bootstrap info is extracted from inlined base64.
 * The <bootstrapInfo> element's scope is the file it resides in.
 * For MLM(multi-level manifest), it's valid only in the stream-level manifest.
 */
class BootstrapInfo
{
public:
    BootstrapInfo() : empty{false} {}
    bool empty; ///< If true it's the default structure, else it's the result of parsing

    std::string id; ///< The ID of this <bootstrapInfo> element. It is optional.
    std::string profile; ///< The profile, or type of bootstrapping represented by this element. It is required. Usually "named"
    std::string url; ///< A URL to a file containing the raw bootstrap info.
    std::vector<uint8_t> data; ///< Raw bootstrap info
};

/*! \brief The <media> element represents one representation of the piece of
 * media. Each representation of the same piece of media has a
 * corresponding <media> element. There must be at least one <media> element.
 *
 */
class Media
{
public:
    Media() : width{0}, height{0}, alternate{false} {
        bootstrapInfo.empty = true;
        drmAdditionalHeader.empty = true;
        dvrInfo.empty = true;
    }

    std::string bitrate; ///< The bitrate of the media file, in kilobits per second.
    int width; ///< The intrinsic width of the media file, in pixels. It is optional.
    int height; ///< The intrinsic height of the media file, in pixels. It is optional.
    std::string streamId; ///< The identifier for media file. It is optional.
    std::string url; ///< The URL of the media file.
    std::string href; ///< The URL of an external F4M file. It is optional. Used only during parsing. F4M 2.0 only
    std::vector<uint8_t> metadata; ///< The <metadata> element represents the stream metadata. It is optional.
    std::string  bootstrapInfoId; ///< The ID of a <bootstrapInfo> element
    BootstrapInfo bootstrapInfo; ///< The bootstrapInfo associated with this media.
    std::string  drmAdditionalHeaderId; ///< The ID of a <drmAdditionalHeader> element. It is optional.
    DrmAdditionalHeader drmAdditionalHeader; ///< The drmAdditionalHeader associated with this media.
    bool alternate; ///< Indicate if this representation is an alternate version. Fixed value to true. It is optional.
    std::string type; ///< The type for alternative track. Valid values include "audio+video", "video", "audio", "data" and "text". It is optional.
    std::string label; ///< The description for alternative track. It is required only if the alternate attribute is present.
    std::string lang; ///< The language code for alternative track. It is required only if the alternate attribute is present.
    std::string groupspec; ///< The group specifier for multicast media. It is optional. Only with multicastStreamName and a RTMFP url.
    std::string multicastStreamName; ///< The stream name for multicast media. It is optional. Only with groupspec and aRTMFP url.
    std::vector<uint8_t> xmpMetadata; ///< The <xmpMetadata> element represents the XMP metadata. F4M 1.0 only
    std::vector<uint8_t> moov; ///< The <moov> element represents the Movie Box, or "moov" atom. F4M 1.0 only
    std::string dvrInfoId; ///< The ID of a <dvrInfo> element. F4M 1.0 only
    DvrInfo dvrInfo; ///< The dvrInfo associated with this media.
};

/*! \brief The root element in the f4m document.
 *
 *  Contains elements valid for every <media>.
 */
class Manifest
{
public:
    Manifest() : duration{0.} {}

    std::string id; ///< The <id> element represents a unique identifier for the media. It is optional.
    float duration; ///< The <duration> element represents the duration of the media, in seconds. Usually for live content, it is 0. It is optional.
    std::string startTime; ///< The <startTime> element represents the date/time at which the media was
        /// first (or will first be) made available. It is optional.
    std::string mimeType; ///< The <mimeType> element represents the MIME type of the media file. It is optional.
    std::string streamType; ///< The <streamType> element is a string representing the way in which the media is streamed.
        /// Valid values include "live", "recorded", and "liveOrRecorded". It is optional.
    std::string deliveryType; ///< The <deliveryType> element indicates the means by which content is delivered to the player.
        /// Valid values include "streaming" and "progressive". It is optional. If unspecified, it is inferred from the media protocol.
    std::vector<Media> medias; ///< All the medias elements associated with parsed f4m file.
    std::string label; ///< The <label> element is a string representing the default user-friendly description of the media.
    std::string lang; ///< The <lang> element is a string representing the base language of the piece of media.
    std::string baseURL; ///< The <baseURL> element contains the base URL for all relative (HTTP-based) URLs in the manifest. It is optional.
};

#endif // MANIFEST_H

