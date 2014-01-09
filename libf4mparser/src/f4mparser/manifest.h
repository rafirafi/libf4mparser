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
    DrmAdditionalHeader() : empty{false}, prefetchDeadline{-1.}, startTimestamp{-1.} {}
    bool empty; ///< If true it's the default structure, else it's the result of parsing

    std::string id; ///< The ID of this <drmAdditionalHeader> element. It is optional
    std::string url; ///< A URL to a file containing the raw DRM AdditionalHeader.
        /// Either the url attribute or the inline BASE64 header (but not both) must be specified.
    std::vector<uint8_t> data; ///< The raw DRM AdditionalHeader

    double prefetchDeadline; ///< Since F4M 3.0
    double startTimestamp; ///< Since F4M 3.0
};

/*! \brief The <dvrInfo> element represents all information needed to play DVR
 * media. It contains no content, only attributes. It is optional.
 *
 * For MLM(multi-level manifest), DvrInfo in stream-level manifest should be ignored.
 */
class DvrInfo
{
public:
    DvrInfo() : empty{false}, beginOffset{-1}, endOffset{-1}, offline{false}, windowDuration{-1} {}
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
    BootstrapInfo() : empty{false}, fragmentDuration{-1.}, segmentDuration{-1.} {}
    bool empty; ///< If true it's the default structure, else it's the result of parsing

    std::string id; ///< The ID of this <bootstrapInfo> element. It is optional.
    std::string profile; ///< The profile, or type of bootstrapping represented by this element. It is required. Usually "named"
    std::string url; ///< A URL to a file containing the raw bootstrap info.
    std::vector<uint8_t> data; ///< Raw bootstrap info

    double fragmentDuration; ///< F4M 3.0 the 'ideal fragment duration', optional
    double segmentDuration; ///< F4M 3.0 the 'ideal segment duration', optional
};

/*! \brief Represent a single sample that maps a stream presentation time to a SMPTE time code.
 * stream-level manifests only.
 * F4M 3.0 only
 */
class SmpteTimecode
{
public:
    SmpteTimecode() : timestamp{-1.0} {}
    std::string smpte; ///< mandatory, format : “hour:minute:second:frame”
    double timestamp; ///< mandatory, format : decimal number of seconds
    std::string date; ///< optional, format : "YYYY-MM-DD"
    std::string timezone; ///< optional, format : "[+/-]hh:mm"
};

/*! \brief convey a splice, a sequence of time within the presentation where content may be inserted.
 * Essentially for helping the client to insert advertising content.
 * F4M 3.0 only
 */
class Cue
{
public:
    Cue() : availNum{-1}, availsExpected{-1}, duration{-1.0}, time{-1.0} {}
    int availNum; ///<  optional, the index of the avail within the total set of avails for the program content.
    int availsExpected; ///<  optional, the expected total number of avails for the program content.
    double duration; ///<  mandatory, the splice duration, expressed as a decimal number of seconds.
    std::string id; ///<  mandatory, the ID of this <cue> element.
    double time; ///<  mandatory, the stream presentation time at which point the splice should occur, expressed as a decimal number of seconds.
    std::string type; ///<  mandatory, legal value : "spliceOut"
    std::string programId; ///<  optional, an identifier for the program content.
};


/*! \brief Contain information needed to enable best-effort fetch support on HTTP streamed media.
 * only set level, multiple instance possible : only one for an adaptive set if id not present, apply to all medias
 * Only to consider if not in bootrapinfo
 * F4M 3.0 only
 */
class BestEffortFetchInfo
{
public:
    BestEffortFetchInfo() : empty{false}, fragmentDuration{-1.}, segmentDuration{-1.} {}
    bool empty;

    std::string id; ///<  optional
    double fragmentDuration; ///<  deprecated
    double segmentDuration; ///<  deprecated
};

/*! \brief The <media> element represents one representation of the piece of
 * media. Each representation of the same piece of media has a
 * corresponding <media> element. There must be at least one <media> element.
 *
 */
class Media
{
public:
    Media() : width{-1}, height{-1}, alternate{false} {
        bootstrapInfo.empty = true;
        drmAdditionalHeader.empty = true;
        dvrInfo.empty = true;
        bestEffortFetchInfo.empty = true;
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
    std::string type; ///< The type for alternative track. Valid values include "audio+video", "video", "audio", "data" and "text". It is optional. F4M 3.0 add the "video-keyframe-only" type.
    std::string label; ///< The description for alternative track. It is required only if the alternate attribute is present.
    std::string lang; ///< The language code for alternative track. It is required only if the alternate attribute is present.
    std::string groupspec; ///< The group specifier for multicast media. It is optional. Only with multicastStreamName and a RTMFP url.
    std::string multicastStreamName; ///< The stream name for multicast media. It is optional. Only with groupspec and aRTMFP url.
    std::vector<uint8_t> xmpMetadata; ///< The <xmpMetadata> element represents the XMP metadata. F4M 1.0 only
    std::vector<uint8_t> moov; ///< The <moov> element represents the Movie Box, or "moov" atom. F4M 1.0 only
    std::string dvrInfoId; ///< The ID of a <dvrInfo> element. F4M 1.0 only
    DvrInfo dvrInfo; ///< The dvrInfo associated with this media.

    std::string audioCodec; ///< The audioCodec for alternative audio track. Only if the alternate attribute is present AND the type is audio. Follow RFC 6381 . Since F4M 3.0. Since F4M 3.0
    std::string videoCodec; ///< ONLY valid if type is "video" or "video-keyframe-only" or "audio+video". Follow RFC6381. Since F4M 3.0.

    std::string cueInfoId; ///< The ID of a <cueInfo> element. Since F4M 3.0
    std::vector<Cue> cueInfo; ///< The collection of Cue associated with the media. Since F4M 3.0

    std::string bestEffortFetchInfoId; ///< The ID of a <bestEffortFetchInfo> element. Since F4M 3.0
    BestEffortFetchInfo bestEffortFetchInfo; ///< store 'ideal' fragment and segment duration, deprecated. Since F4M 3.0

    std::string drmAdditionalHeaderSetId; ///< F4M 3.0, to link several drmAdditionalHeader to a media
                                          ///< drmAdditionalHeaderId must not be present if this one is : exclusive
    std::vector<DrmAdditionalHeader> drmAdditionalHeaderSet; ///< For license rotation.

    std::vector<SmpteTimecode> smpteTimeCodes; ///< . Since F4M 3.0
};

/*! \brief Back-up/additionnal definition for medias
 *
 *  Contains an explicit adaptive set.
 */
class AdaptiveSet
{
public:
    std::vector<Media> medias;
};

/*! \brief The root element in the f4m document.
 *
 *  Contains elements valid for every <media>.
 */
class Manifest
{
public:
    Manifest() : duration{-1.} {}

    std::string id; ///< The <id> element represents a unique identifier for the media. It is optional.
    double duration; ///< The <duration> element represents the duration of the media, in seconds. Usually for live content, it is 0. It is optional.
    std::string startTime; ///< The <startTime> element represents the date/time at which the media was
        /// first (or will first be) made available. It is optional.
    std::string mimeType; ///< The <mimeType> element represents the MIME type of the media file. It is optional.
    std::string streamType; ///< The <streamType> element is a string representing the way in which the media is streamed.
        /// Valid values include "live", "recorded", and "liveOrRecorded". It is optional.
    std::string deliveryType; ///< The <deliveryType> element indicates the means by which content is delivered to the player.
        /// Valid values include "streaming" and "progressive". It is optional. If unspecified, it is inferred from the media protocol.
    std::vector<Media> medias; ///< All the medias elements associated with parsed f4m file. Part of the 'implicit' adaptive set.
    std::string label; ///< The <label> element is a string representing the default user-friendly description of the media.
    std::string lang; ///< The <lang> element is a string representing the base language of the piece of media.
    std::string baseURL; ///< The <baseURL> element contains the base URL for all relative (HTTP-based) URLs in the manifest. It is optional.

    std::vector<std::string> profiles; ///< F4M 3.0 spec (says 2.0) , defaut to "urn://profile.adobe.com/F4F", list of hds profiles supported, each separated by a space. URN as specified in [RFC2142]

    std::vector<AdaptiveSet> adaptiveSets; ///< F4M 3.0 only : 'explicit' adaptive sets.
};

#endif // MANIFEST_H

