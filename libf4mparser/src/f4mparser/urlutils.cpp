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

#include "urlutils.h"

#include <cctype> //tolower

namespace UrlUtils
{

bool isAbsolute(const std::string &url)
{
    return url.find("://") != std::string::npos;
}

bool haveHttpScheme(const std::string &url)
{
    const char * scheme = "http";

    if (url.empty() || url.size() < 4) {
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (tolower(url.at(i)) != scheme[i]) {
            return false;
        }
    }
    return true;
}

bool haveRtmfpScheme(const std::string &url)
{
    const char * scheme = "rtmfp";

    if (url.empty() || url.size() < 5) {
        return false;
    }

    for (int i = 0; i < 5; ++i) {
        if (tolower(url.at(i)) != scheme[i]) {
            return false;
        }
    }
    return true;
}

} // namespace UrlUtils
