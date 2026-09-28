/*
 * Copyright (c) 2018, Bertold Van den Bergh (vandenbergh@bertold.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <shapefil.h>
#include <iostream>
#include <limits>
#include <fstream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <math.h>
#include <tuple>
#include <cstdint>

unsigned version = 1;

const double Inf = std::numeric_limits<float>::infinity();

std::unordered_map<std::string, std::string> alpha2ToName;
std::unordered_map<std::string, std::string> tzidToAlpha2;

void errorFatal(std::string what)
{
    std::cerr<<what<<"\n";
    exit(1);
}

void assert_(bool mustBeTrue, std::string what){
    if(!mustBeTrue){
        errorFatal(what);
    }
}

uint64_t encodeSignedToUnsigned(int64_t valueIn){
    uint64_t value = valueIn * 2;
    if(valueIn < 0) {
        value = -valueIn * 2 + 1;
    }

    return value;
}

int encodeVariableLength(std::vector<uint8_t>& output, int64_t valueIn, bool handleNeg = true)
{
    uint64_t value = valueIn;

    if(handleNeg) {
        value = encodeSignedToUnsigned(valueIn);
    }

    int bytesUsed = 0;
    do {
        uint8_t byteOut = value & 0x7F;
        if(value >= 128) {
            byteOut |= 128;
        }
        output.push_back(byteOut);
        bytesUsed ++;
        value >>= 7;
    } while(value);

    return bytesUsed;
}

uint64_t encodePointTo64(int64_t lat, int64_t lon){
    assert_(lat || lon, "Tried to encode 0,0. This is not allowed");

    uint64_t latu=encodeSignedToUnsigned(lat);
    uint64_t lonu=encodeSignedToUnsigned(lon);

    assert_(latu < (uint64_t)1<<32, "Unsigned lat overflow");
    assert_(lonu < (uint64_t)1<<32, "Unsigned lat overflow");

    uint64_t point = 0;
    for(uint8_t i=31; i<=31; i--){
        point <<= 2;
        if(latu & (1<<i)){
            point |= 1;
        }
        if(lonu & (1<<i)){
            point |= 2;
        }
    }

    return point;
}
    

int64_t doubleToFixedPoint(double input, double scale, unsigned int precision = 32)
{
    double inputScaled = input / scale;
    return inputScaled * pow(2, precision-1);

}

struct Point;
struct PolygonData;

std::unordered_map<uint64_t, Point*> pointMap_;

struct Point {
    static Point* GetPoint(double dlat = 0, double dlon = 0, unsigned int precision = 32){    
	int64_t lat = doubleToFixedPoint(dlat, 90, precision);
        int64_t lon = doubleToFixedPoint(dlon, 180, precision);
        
        uint64_t key = encodePointTo64(lat, lon);
        if(pointMap_.count(key)){
            return pointMap_[key];
        }
        
        Point* p = new Point(lat, lon);
        p->key_ = key;
        pointMap_[key] = p;
        return p;
    }

    Point(int64_t lat = 0, int64_t lon = 0)
    {
        lat_ = lat;
        lon_ = lon;   
    }

    std::tuple<int64_t, int64_t> value()
    {
        return std::make_tuple(lat_, lon_);
    }

    int encodePointBinary(std::vector<uint8_t>& output)
    {
        int bytesUsed = encodeVariableLength(output, lat_);
        bytesUsed += encodeVariableLength(output, lon_);

        return bytesUsed;
    }

    int64_t lat_;
    int64_t lon_;
    uint64_t key_;
    PolygonData* parent_ = nullptr;
    int index_ = 0;
    bool encoded_ = false;
    uint64_t encodedOffset_ = 0;
};

struct PolygonData {
    Point boundingMin;
    Point boundingMax;
    std::vector<Point*> points_;
    unsigned long fileIndex_ = 0;
    unsigned long metadataId_;
    Point* lastPoint_ = nullptr;

    void processPoint(Point* p)
    {
        if(p->lat_ < boundingMin.lat_) {
            boundingMin.lat_ = p->lat_;
        }
        if(p->lon_ < boundingMin.lon_) {
            boundingMin.lon_ = p->lon_;
        }
        if(p->lat_ > boundingMax.lat_) {
            boundingMax.lat_ = p->lat_;
        }
        if(p->lon_ > boundingMax.lon_) {
            boundingMax.lon_ = p->lon_;
        }

	/* Don't encode duplicate points */
	if(lastPoint_ == p){
	    return;
	}
	lastPoint_ = p;
	
        points_.push_back(p);
    }

    PolygonData(unsigned long id):
        boundingMin(INT64_MAX, INT64_MAX),
        boundingMax(INT64_MIN, INT64_MIN),
        metadataId_(id)
    {
    }
    

    struct LineSegment {
        std::vector<Point*> points_;
        Point* prevPoint_;
        PolygonData* parent_;
    
        bool sameDirection(int64_t x1, int64_t y1, int64_t x2, int64_t y2){
            if(!x2 && !y2){
                return false;
            }

            if((x1 > 0 && x2 < 0) || (x1 < 0 && x2 > 0)){
                return false;
            }        
            if((y1 > 0 && y2 < 0) || (y1 < 0 && y2 > 0)){
                return false;
            }

            if(x1 == 0){
                return x2 == 0;
            }

            return y2 == (y1*x2/x1);
        }

    
        unsigned int encodeDelta(std::vector<uint8_t>& output, PolygonData* mark = nullptr, int start = 0, int end = -1){
            unsigned int numPoints = 0;
            if(end < 0){
                end = points_.size()-1;
            }

            int64_t accDiffLat = 0, accDiffLon = 0;
            int64_t prevDiffLat = 0, prevDiffLon = 0;

            int64_t prevLat, prevLon;
            
            Point* prevPoint = prevPoint_;
            if(start > 0){
                prevPoint = points_[start-1];
            }

            std::tie(prevLat, prevLon) = prevPoint->value();

            auto encodePoint = [&](bool force = false){
                /* Encode accumulator.
                 * After this the position is equal to that of the previous point */
                if(accDiffLat || accDiffLon || force){
                    if(version == 0){
                        encodeVariableLength(output, accDiffLat);
                        encodeVariableLength(output, accDiffLon);
                    }else{
                        encodeVariableLength(output, encodePointTo64(accDiffLat, accDiffLon), false);
                    }

                    numPoints++;
                }

                /* Mark points as encoded if we mark and we are the parent */
                if(mark && prevPoint->parent_ == mark){
                    prevPoint->encoded_ = true;
                    prevPoint->encodedOffset_ = output.size();
                }

                /* Reset accumulator */
                accDiffLat = 0;
                accDiffLon = 0;
            };

            for(int i = start; i<=end; i++){
                Point* point = points_[i];

                int64_t lat, lon;
                std::tie(lat, lon) = point->value();

                /* Calculate difference */
                int64_t diffLat = lat - prevLat;
                int64_t diffLon = lon - prevLon;
               
                /* Encode delta */
                if(!sameDirection(diffLat, diffLon, prevDiffLat, prevDiffLon)){
                    encodePoint();
                }

                accDiffLat += diffLat;
                accDiffLon += diffLon; 

                /* Store previous values */
                prevDiffLat = diffLat;
                prevDiffLon = diffLon;
                prevLat = lat;
                prevLon = lon;
                prevPoint = point;
            } 
            
            /* Encode remainder if needed */ 
            encodePoint(version == 0);

            return numPoints;
        }
        
        bool encodeReference(std::vector<uint8_t>& output){
            /* Search for first marked point */
            int end = -1, start = -1;
            for(int i=0; i<points_.size(); i++){
                if(points_[i]->encoded_){
                    start = i;
                    break;
                }
            }
            
            for(int i=points_.size()-1; i>=0; i--){
                if(points_[i]->encoded_){
                    end = i;
                    break;
                }
            }

            if(end < 0 || start < 0){
                /* Only unencoded points, then we can only delta encode it ourself */
                return false;
            }

            /* Encode delta until where we can refer */
            encodeDelta(output, nullptr, 0, start);
            

            /* Add reference marker if it is still needed */
            if(start != end){
                uint64_t startRef = points_[start]->encodedOffset_;
                uint64_t endRef = points_[end]->encodedOffset_;

                output.push_back(0);
                output.push_back(1);
                encodeVariableLength(output, startRef, false);
                int64_t diff = endRef - startRef;
                encodeVariableLength(output, diff, true);
            }

            /* Encode delta till the end of the segment */
            encodeDelta(output, nullptr, end+1);

            return true;
        }
    };

    unsigned int encodeBinaryData(std::vector<uint8_t>& output)
    {
        std::vector<LineSegment*> lines_;
        PolygonData* currentParent = nullptr;
        LineSegment* segment = nullptr;

        /* Step 1: Encode first point */
        Point* prevPoint = points_[0];
        if(version == 0){
            prevPoint->encodePointBinary(output);
        }else{
            encodeVariableLength(output, prevPoint->key_, false);
        }

        int direction = 0;
        /* Step 2: Go through the list of points and check which ones already exist.
         * We skip the first and last one since the first one is already encoded
         * and the last one is identical to the first */
        for(int i=1; i<points_.size()-1; i++){
            Point* point = points_[i];

            if(!point->parent_){
                point->parent_ = this;
                point->index_ = i;
            }
            
            bool newSegment = false;

            if(point->parent_ == currentParent){
                if(direction == 0){
                    direction = point->index_ - prevPoint->index_;
                    if(direction > 1 || direction < -1){
                        newSegment = true;
                    }
                }else{
                    if(point->index_ != prevPoint->index_ + direction){
                        newSegment = true;
                    }
                }
            }

            if(point->parent_ != currentParent || newSegment){
                if(segment){
                    lines_.push_back(segment);
                }
                
                currentParent = point->parent_;

                segment = new LineSegment();
                segment->prevPoint_ = prevPoint;
                segment->parent_ = currentParent;
                direction = 0;
            }

            segment->points_.push_back(point);

            prevPoint = point;
        }
        if(segment){
            lines_.push_back(segment);
        }

        unsigned int v0Points = 1;

        /* Step 3: Encode segments */
        for(LineSegment* segment: lines_){
            if(segment->parent_ == this || version == 0){
                /* If we are the parent of the segment we must encode and mark it */
                v0Points += segment->encodeDelta(output, this);
            }else{
                /* We are not the parent, we can encode it or refer to it, depending on 
                 * which takes less bytes. In any case we should not mark it. */
                std::vector<uint8_t> delta;
                segment->encodeDelta(delta);
                
                std::vector<uint8_t> reference;
                bool possible = segment->encodeReference(reference);

                if(!possible || delta.size() <= reference.size()){
                    output.insert(std::end(output), std::begin(delta), std::end(delta));
                }else{
                    output.insert(std::end(output), std::begin(reference), std::end(reference));
                }
            }
        }

        if (version != 0){
            /* Step 4: Write end marker */
            output.push_back(0);
            output.push_back(0);
        }

        return v0Points;
    }
};

void encodeStringToBinary(std::vector<uint8_t>& output, std::string& input)
{
    encodeVariableLength(output, input.size(), false);
    for(unsigned int i=0; i<input.size(); i++) {
        output.push_back(input[i] ^ 0x80);
    }
}


std::unordered_map<std::string, uint64_t> usedStrings_;

struct MetaData {
    void encodeBinaryData(std::vector<uint8_t>& output)
    {
        for(std::string& str: data_) {
            if(str.length() >= 256) {
                std::cout << "Metadata string is too long\n";
                exit(1);
            }

            if(!usedStrings_.count(str)) {
                usedStrings_[str] = output.size();
                encodeStringToBinary(output, str);
            } else {
                encodeVariableLength(output, usedStrings_[str] + 256, false);
            }
        }
    }

    std::vector<std::string> data_;

    unsigned long fileIndex_;
};


std::vector<PolygonData*> polygons_;
std::vector<MetaData> metadata_;
std::vector<std::string> fieldNames_;


unsigned int decodeVariableLength(uint8_t* buffer, int64_t* result, bool handleNeg = true)
{
    int64_t value = 0;
    unsigned int i=0, shift = 0;

    do {
        value |= (buffer[i] & 0x7F) << shift;
        shift += 7;
    } while(buffer[i++] & 0x80);

    if(!handleNeg) {
        *result = value;
    } else {
        *result = (value & 1)?-(value/2):(value/2);
    }
    return i;
}

void readMetaDataTimezone(DBFHandle dataHandle)
{
    /* Specify field names */
    fieldNames_.push_back("TimezoneIdPrefix");
    fieldNames_.push_back("TimezoneId");
    fieldNames_.push_back("CountryAlpha2");
    fieldNames_.push_back("CountryName");

    /* Parse attribute names */
    for(int i = 0; i < DBFGetRecordCount(dataHandle); i++) {
        metadata_[i].data_.resize(4);
        for(int j = 0; j < DBFGetFieldCount(dataHandle); j++) {
            char fieldTitle[12];
            int fieldWidth, fieldDecimals;
            DBFFieldType eType = DBFGetFieldInfo(dataHandle, j, fieldTitle, &fieldWidth, &fieldDecimals);

            fieldTitle[11] = 0;
            std::string fieldTitleStr(fieldTitle);

            if( eType == FTString ) {
                if(fieldTitleStr == "tzid") {
                    std::string data = DBFReadStringAttribute(dataHandle, i, j);
                    size_t pos = data.find('/');
                    if (pos == std::string::npos) {
                        metadata_[i].data_.at(0) = data;
                    } else {
                        metadata_[i].data_.at(0) = data.substr(0, pos) + "/";
                        metadata_[i].data_.at(1) = data.substr(pos + 1, std::string::npos);
                    }
                    if(tzidToAlpha2.count(data)) {
                        metadata_[i].data_.at(2) = tzidToAlpha2[data];
                        if(alpha2ToName.count(metadata_[i].data_.at(2))) {
                            metadata_[i].data_.at(3) = alpha2ToName[metadata_[i].data_.at(2)];
                        } else {
                            std::cout<<metadata_[i].data_.at(2)<< " not found in alpha2ToName! ("<<data<<")\n";
                        }
                    } else {
                        std::cout<<data<<" not found in zoneToAlpha2!\n";
                    }
                }
            }
        }
    }
}

void readMetaDataNaturalEarthCountry(DBFHandle dataHandle)
{
    /* Specify field names */
    fieldNames_.push_back("Alpha2");
    fieldNames_.push_back("Alpha3");
    fieldNames_.push_back("Name");

    /* Parse attribute names */
    for(int i = 0; i < DBFGetRecordCount(dataHandle); i++) {
        metadata_[i].data_.resize(3);
        for(int j = 0; j < DBFGetFieldCount(dataHandle); j++) {
            char fieldTitle[12];
            int fieldWidth, fieldDecimals;
            DBFFieldType eType = DBFGetFieldInfo(dataHandle, j, fieldTitle, &fieldWidth, &fieldDecimals);

            fieldTitle[11] = 0;
            std::string fieldTitleStr(fieldTitle);

            if( eType == FTString ) {
                if(fieldTitleStr == "ISO_A2" || fieldTitleStr == "WB_A2") {
                    std::string tmp = DBFReadStringAttribute(dataHandle, i, j);
                    if(tmp != "-99") {
                        metadata_[i].data_.at(0) = tmp;
                    }
                } else if(fieldTitleStr == "ISO_A3" || fieldTitleStr == "WB_A3" || fieldTitleStr == "BRK_A3") {
                    std::string tmp = DBFReadStringAttribute(dataHandle, i, j);
                    if(tmp != "-99") {
                        metadata_[i].data_.at(1) = tmp;
                    }
                } else if(fieldTitleStr == "NAME_LONG") {
                    metadata_[i].data_.at(2) = DBFReadStringAttribute(dataHandle, i, j);
                }
            }

        }
    }
}

std::unordered_map<std::string, std::string> parseAlpha2ToName(DBFHandle dataHandle)
{
    std::unordered_map<std::string, std::string> result;

    for(int i = 0; i < DBFGetRecordCount(dataHandle); i++) {
        std::string alpha2, name;
        for(int j = 0; j < DBFGetFieldCount(dataHandle); j++) {
            char fieldTitle[12];
            int fieldWidth, fieldDecimals;
            DBFFieldType eType = DBFGetFieldInfo(dataHandle, j, fieldTitle, &fieldWidth, &fieldDecimals);

            fieldTitle[11] = 0;
            std::string fieldTitleStr(fieldTitle);

            if( eType == FTString ) {
                if(fieldTitleStr == "ISO_A2" || fieldTitleStr == "WB_A2") {
                    std::string tmp = DBFReadStringAttribute(dataHandle, i, j);
                    if(tmp != "-99" && alpha2 == "") {
                        alpha2 = tmp;
                    }
                } else if(fieldTitleStr == "NAME_LONG") {
                    name = DBFReadStringAttribute(dataHandle, i, j);
                }
            }
        }
        if(alpha2 != "") {
            result[alpha2]=name;
        }
    }

    result["GF"]="French Guiana";
    result["GP"]="Guadeloupe";
    result["BQ"]="Bonaire";
    result["MQ"]="Martinique";
    result["SJ"]="Svalbard and Jan Mayen Islands";
    result["NO"]="Norway";
    result["CX"]="Christmas Island";
    result["CC"]="Cocos Islands";
    result["YT"]="Mayotte";
    result["RE"]="Réunion";
    result["TK"]="Tokelau";
    result["TW"]="Taiwan";

    return result;
}

std::unordered_map<std::string, std::string> parseTimezoneToAlpha2(std::string path)
{
    std::unordered_map<std::string, std::string> result;
    //TODO: Clean solution...
#include "zoneToAlpha.h"

    return result;
}

int main(int argc, char ** argv )
{
    if(argc != 7) {
        std::cout << "Wrong number of parameters\n";
        return 1;
    }

    tzidToAlpha2 = parseTimezoneToAlpha2("TODO");

    char tableType = argv[1][0];
    std::string path = argv[2];
    std::string outPath = argv[3];
    unsigned int precision = strtol(argv[4], NULL, 10);
    std::string notice = argv[5];
    version = strtol(argv[6], NULL, 10);
    if(version > 1){
        std::cout << "Unknown version\n";
        return 1;
    }

    DBFHandle dataHandle = DBFOpen("naturalearth/ne_10m_admin_0_countries_lakes", "rb" );
    alpha2ToName = parseAlpha2ToName(dataHandle);
    DBFClose(dataHandle);

    dataHandle = DBFOpen(path.c_str(), "rb" );
    if( dataHandle == NULL ) {
        errorFatal("Could not open attribute file\n");
    }

    metadata_.resize(DBFGetRecordCount(dataHandle));
    std::cout << "Reading "<<metadata_.size()<<" metadata records.\n";

    if(tableType == 'C') {
        readMetaDataNaturalEarthCountry(dataHandle);
    } else if(tableType == 'T') {
        readMetaDataTimezone(dataHandle);
    } else {
        std::cout << "Unknown table type\n";
        return 1;
    }

    DBFClose(dataHandle);

    SHPHandle shapeHandle = SHPOpen(path.c_str(), "rb");
    if( shapeHandle == NULL ) {
        errorFatal("Could not open shapefile\n");
    }

    int numEntities, shapeType, totalPolygons = 0;
    SHPGetInfo(shapeHandle, &numEntities, &shapeType, NULL, NULL);

    std::cout<<"Opened "<<SHPTypeName( shapeType )<< " file with "<<numEntities<<" entries.\n";

    for(int i = 0; i < numEntities; i++ ) {
        SHPObject *shapeObject;

        shapeObject = SHPReadObject( shapeHandle, i );
        if(shapeObject) {
            if(shapeObject->nSHPType != 3 && shapeObject->nSHPType != 5 &&
                    shapeObject->nSHPType != 13 && shapeObject->nSHPType != 15) {
                std::cout<<"Unsupported shape object ("<< SHPTypeName(shapeObject->nSHPType) <<")\n";
                continue;
            }

            int partIndex = 0;

            PolygonData* polygonData = nullptr;

            for(int j = 0; j < shapeObject->nVertices; j++ ) {
                if(j == 0 || j == shapeObject->panPartStart[partIndex]) {
                    totalPolygons++;

                    if(polygonData) {
                        /* Commit it */
                        polygons_.push_back(polygonData);
                    }
                    polygonData =  new PolygonData(i);

                    if(partIndex + 1 < shapeObject->nParts) {
                        partIndex++;
                    }
                }

                Point* p = Point::GetPoint(shapeObject->padfY[j], shapeObject->padfX[j], precision);
                polygonData->processPoint(p);

            }

            if(polygonData) {
                /* Commit it */
                polygons_.push_back(polygonData);
            }

            SHPDestroyObject(shapeObject);
        }
    }

    SHPClose(shapeHandle);

    std::cout<<"Parsed "<<totalPolygons<<" polygons.\n";

    /* Sort according to bounding box */
    std::sort(polygons_.begin(), polygons_.end(), [](PolygonData* a, PolygonData* b) {
        return a->boundingMin.lat_ < b->boundingMin.lat_;
    });

    /* Encode data section and store pointers */
    std::vector<uint8_t> outputData;
    for(PolygonData* polygon: polygons_) {
        polygon->fileIndex_ = outputData.size();
        if(version == 0){
            std::vector<uint8_t> tmpData;
            unsigned int numPoints = polygon->encodeBinaryData(tmpData);
            encodeVariableLength(outputData, numPoints, false);
            outputData.insert(std::end(outputData), std::begin(tmpData), std::end(tmpData));
        }else{
            polygon->encodeBinaryData(outputData);
        }
    }
    std::cout << "Encoded data section into "<<outputData.size()<<" bytes.\n";

    /* Encode metadata */
    std::vector<uint8_t> outputMeta;
    for(MetaData& metadata: metadata_) {
        metadata.fileIndex_ = outputMeta.size();
        metadata.encodeBinaryData(outputMeta);
    }
    std::cout << "Encoded metadata into "<<outputMeta.size()<<" bytes.\n";

    /* Encode bounding boxes */
    std::vector<uint8_t> outputBBox;
    int64_t prevFileIndex = 0;
    int64_t prevMetaIndex = 0;
    for(PolygonData* polygon: polygons_) {
        polygon->boundingMin.encodePointBinary(outputBBox);
        polygon->boundingMax.encodePointBinary(outputBBox);

        encodeVariableLength(outputBBox, metadata_.at(polygon->metadataId_).fileIndex_ - prevMetaIndex);
        prevMetaIndex = metadata_[polygon->metadataId_].fileIndex_;

        encodeVariableLength(outputBBox, polygon->fileIndex_ - prevFileIndex, false);
        prevFileIndex = polygon->fileIndex_;
    }
    std::cout << "Encoded bounding box section into "<<outputBBox.size()<<" bytes.\n";

    /* Encode header */
    std::vector<uint8_t> outputHeader;
    outputHeader.push_back('P');
    outputHeader.push_back('L');
    outputHeader.push_back('B');
    outputHeader.push_back(tableType);
    outputHeader.push_back(version);
    outputHeader.push_back(precision);
    outputHeader.push_back(fieldNames_.size());
    for(unsigned int i=0; i<fieldNames_.size(); i++) {
        encodeStringToBinary(outputHeader, fieldNames_[i]);
    }
    encodeStringToBinary(outputHeader, notice);
    encodeVariableLength(outputHeader, outputBBox.size(), false);
    encodeVariableLength(outputHeader, outputMeta.size(), false);
    encodeVariableLength(outputHeader, outputData.size(), false);
    std::cout << "Encoded header into "<<outputHeader.size()<<" bytes.\n";

    FILE* outputFile = fopen(outPath.c_str(), "wb");
    fwrite(outputHeader.data(), 1, outputHeader.size(), outputFile);
    fwrite(outputBBox.data(), 1, outputBBox.size(), outputFile);
    fwrite(outputMeta.data(), 1, outputMeta.size(), outputFile);
    fwrite(outputData.data(), 1, outputData.size(), outputFile);
    fclose(outputFile);

}
