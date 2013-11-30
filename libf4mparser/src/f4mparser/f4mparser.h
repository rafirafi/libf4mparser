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

/*! \file f4mparser.h
 *  \brief Contains the interfaces for the manifest parser.
 *
 *  This contains the interfaces needed to parse a manifest from an url
 *  and to update a dvrInfo structure.
 *
 *  \author (rafirafi)
 */

#ifndef F4MPARSER_H
#define F4MPARSER_H

#include "manifest.h"

#pragma GCC visibility push(default)

/*! \brief This is the format which must be provided for the downloading file  function pointer.
 *
 * First parameter is to give back the user pointer to the callee
 * Second parameter is the url
 * Third parameter should contains the http code of the request (or -1 if another error occured)
 * Returns the data in a vector of unsigned char.
 *
 * example of a complete signature :
 * std::vector<uint8_t> myDownloadFunction(void *userPtr, std::string url, long &httpStatusCode)
 *
*/
typedef std::vector<uint8_t>(*DOWNLOAD_FILE_FUNCTION)(void *, std::string, long &);

/*! \brief retrieve the medias information from a http pointing to a f4m document.
 *
 * \param[in]  downloadFileUserPtr  A user pointer passed with callback function when downloading a file, or NULL
 * \param[in]  downloadFileFctPtr   A callback function for downloading a file
 * \param[in]  url                  The url pointing to the manifest file
 * \param[out] manifest             The structure containing all the valid informations for each media described in the manifest file
 * \return bool                     Returns true if the parsing was successfull
*/
bool F4mParseManifest(void *downloadFileUserPtr,
                      DOWNLOAD_FILE_FUNCTION downloadFileFctPtr,
                      const std::string &url,
                      Manifest *manifest);

/*! \brief retrieve the medias information from a http pointing to a dvr xml document.
 *
 * \param[in]  downloadFileUserPtr  A user pointer passed with callback function when downloading a file
 * \param[in]  downloadFileFctPtr   A callback function for downloading a file
 * \param[in]  url                  The url pointing to the dvrInfo xml document
 * \param[out] dvrInfo              A dvrInfo structure
 * \return bool                     Returns true if the dvrInfo xml document was parsed
*/
bool F4mUpdateDvrInfo(void *downloadFileUserPtr,
                      DOWNLOAD_FILE_FUNCTION downloadFileFctPtr,
                      const std::string &url,
                      DvrInfo *dvrInfo);

#pragma GCC visibility pop

#endif // F4MPARSER_H
