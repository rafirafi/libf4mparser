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

#include "base64utils.h" // Base64Utils

#ifndef F4M_DEBUG
#define F4M_DLOG(x)
#else
#include <iostream> // std::cerr
#define F4M_DLOG(x) x
#endif

std::string ManifestParser::getNodeAttributeValueAsString(const xmlAttrPtr attribute)
{
    std::string str;
    if (attribute && attribute->children) {
        xmlChar* attrStr = xmlNodeListGetString(m_manifestDoc->m_doc, attribute->children, 1);
        if (attrStr) {
            str = (const char *)attrStr;
            str.erase(remove_if(begin(str), end(str), isspace), end(str)); //trim
            F4M_DLOG(std::cerr << __func__  << " [" << attribute->name << "] = " + str << std::endl;)
            xmlFree(attrStr);
        }
    }
    return str;
}

int ManifestParser::getNodeAttributeValueAsInt(const xmlAttrPtr attribute)
{
    int result = 0;
    std::string str = getNodeAttributeValueAsString(attribute);
    if (str.empty()) {
        return result;
    }
    try { result = std::stoi(str); } catch (...) {
        F4M_DLOG(std::cerr << __func__ <<  " [" << attribute->name << "] " << "not an int "
                  << result << std::endl;)
        result = 0; } // !

    return result;
}

float ManifestParser::getNodeAttributeValueAsFloat(const xmlAttrPtr attribute)
{
    float result = 0.;
    std::string str = getNodeAttributeValueAsString(attribute);
    if (str.empty()) {
        return result;
    }
    try { result = std::stof(str); } catch (...) {
        F4M_DLOG(std::cerr << __func__ <<  " [" << attribute->name << "] " << "not a float "
                  << result << std::endl;)
        result = 0.; } // !

    return result;
}

std::string ManifestParser::getNodeContentAsString(const xmlNodePtr node)
{
    std::string str;
    if (node) {
        xmlChar* content = xmlNodeGetContent(node);
        if (content) {
            str = (char *)content;
            str.erase(remove_if(begin(str), end(str), isspace), end(str)); //trim
            F4M_DLOG(std::cerr << __func__  << " [" << node->name << "] = " + str << std::endl;)
            xmlFree(content);
        }
    }
    return str;
}

int ManifestParser::getNodeContentAsInt(const xmlNodePtr node)
{
    int result = 0;
    std::string str = getNodeContentAsString(node);
    if (str.empty()) {
        return result;
    }
    try { result = std::stoi(str); } catch (...) {
        F4M_DLOG(std::cerr << __func__ <<  " [" << node->name << "] " << "not an int "
                  << result << std::endl;)
        result = 0; } // !

    return result;
}

float ManifestParser::getNodeContentAsFloat(const xmlNodePtr node)
{
    float result = 0.;
    std::string str = getNodeContentAsString(node);
    if (str.empty()) {
        return result;
    }
    try { result = std::stoi(str); } catch (...) {
        F4M_DLOG(std::cerr << __func__ <<  " [" << node->name << "] " << "not a float "
                  << result << std::endl;)
        result = 0.; } // !

    return result;
}

bool ManifestParser::nodeIsInNsF4m(const xmlNodePtr node)
{
    return (node && node->ns && node->ns->href &&
            strcmp((const char *)node->ns->href, m_manifestDoc->m_nsF4m) == 0);
}

std::vector<uint8_t> ManifestParser::getNodeBase64Data(const xmlNodePtr node)
{
    std::vector<uint8_t> data;
    if (node) {
        xmlChar* content = xmlNodeGetContent(node);
        if (content) {
            std::string str = (char *)content;
            xmlFree(content);
            str.erase(remove_if(begin(str), end(str), isspace), end(str)); //trim
            data = Base64Utils::decode(str);
        }
    }
    return data;
}
