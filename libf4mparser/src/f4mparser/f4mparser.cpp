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

#include "f4mparser.h"

#include "manifestparser.h"

bool F4mParseManifest(void *downloadFileUserPtr, DOWNLOAD_FILE_FUNCTION downloadFileFctPtr,
                      const std::string &url, Manifest *manifest)
{
    ManifestParser manifestParser(downloadFileUserPtr, downloadFileFctPtr);
    return manifestParser.parse(url, manifest);
}

bool F4mUpdateDvrInfo(void *downloadFileUserPtr, DOWNLOAD_FILE_FUNCTION downloadFileFctPtr,
                      const std::string &url, DvrInfo *dvrInfo)
{
    return ManifestParser::updateDvrInfo(downloadFileUserPtr, downloadFileFctPtr, url, dvrInfo);
}
