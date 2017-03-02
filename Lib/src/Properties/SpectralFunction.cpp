/* Copyright 2016 Kristofer Björnson
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file SpectralFunction.cpp
 *
 *  @author Kristofer Björnson
 */

#include "SpectralFunction.h"

#include <utility>

namespace TBTK{
namespace Property{

SpectralFunction::SpectralFunction(
	int dimensions,
	const int *ranges,
	double lowerBound,
	double upperBound,
	int resolution
) : LDOS(dimensions, ranges, lowerBound, upperBound, resolution){
}

SpectralFunction::SpectralFunction(
	int dimensions,
	const int *ranges,
	double lowerBound,
	double upperBound,
	int resolution,
	const double *data
) : LDOS(dimensions, ranges, lowerBound, upperBound, resolution, data){
}

SpectralFunction::SpectralFunction(
	const SpectralFunction &spectralFunction
) :
	LDOS(spectralFunction)
{
}

SpectralFunction::SpectralFunction(
	SpectralFunction &&spectralFunction
) :
	LDOS(std::move(spectralFunction))
{
}

SpectralFunction::~SpectralFunction(){
}

SpectralFunction& SpectralFunction::operator=(const SpectralFunction &rhs){
	LDOS::operator=(rhs);
	return *this;
}

SpectralFunction& SpectralFunction::operator=(SpectralFunction &&rhs){
	LDOS::operator=(std::move(rhs));
	return *this;
}

};	//End of namespace Property
};	//End of namespace TBTK