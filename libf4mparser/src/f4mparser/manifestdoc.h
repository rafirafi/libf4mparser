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

#ifndef MANIFESTDOC_H
#define MANIFESTDOC_H

#include <libxml/xpath.h>
#include <string>

class ManifestDoc
{
public:
    explicit ManifestDoc(std::string url);
    ~ManifestDoc();
    friend class ManifestParser;
private:
    std::string m_fileUrl;
    xmlDocPtr m_doc;
    xmlXPathContextPtr m_xpathCtx;
    const char *m_nsF4m;
    bool m_isMultiLevelStreamLevel;

    int getF4mVersion() const;

    static const char* const m_nsF4mVersion1;
    static const char* const m_nsF4mVersion2;
};

#endif // MANIFESTDOC_H
