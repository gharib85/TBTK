/* Copyright 2019 Kristofer Björnson
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

/** @package TBTKcalc
 *  @file Integer.h
 *  @brief Integer number.
 *
 *  @author Kristofer Björnson
 */

#ifndef COM_DAFER45_TBTK_INTEGER
#define COM_DAFER45_TBTK_INTEGER

#include "TBTK/PseudoSerializable.h"

namespace TBTK{

/** @brief Integer number. */
class Integer : PseudoSerializable{
public:
	/** Constructor. */
	Integer(){};

	/** Constructor.
	 *
	 *  @param value The value to initilize the Integer with. */
	constexpr Integer(int value) : value(value) {}

	/** Constructs an Index from a serialization string.
	 *
	 *  @param serialization Serialization string from which to construct
	 *  the Integer.
	 *
	 *  @param mode Mode with which the string has been serialized. */
	Integer(const std::string &serialization, Serializable::Mode mode);

	/** Type conversion operator. */
	constexpr operator int() const{	return value;	};

	/** Assignment operator.
	 *
	 *  @param value The value to assign the Integer.
	 *
	 *  @return The Integer after assignment has occured. */
	Integer& operator=(int rhs){
		value = rhs;

		return *this;
	}

	/** Addition assignment operator.
	 *
	 *  @param rhs The right hand side.
	 *
	 *  @return The Integer after the addition has occured. */
	Integer& operator+=(const Integer &rhs){
		value += rhs.value;

		return *this;
	}

	/** Subtraction assignment operator.
	 *
	 *  @param rhs The right hand side.
	 *
	 *  @return The Integer after the subtraction has occured. */
	Integer& operator-=(const Integer &rhs){
		value -= rhs.value;

		return *this;
	}

	/** Multiplication assignment operator.
	 *
	 *  @param rhs The right hand side.
	 *
	 *  @return The Integer after the multiplication has occured. */
	Integer& operator*=(const Integer &rhs){
		value *= rhs.value;

		return *this;
	}

	/** Division assignment operator.
	 *
	 *  @param rhs The right hand side.
	 *
	 *  @return The Integer after the division has occured. */
	Integer& operator/=(const Integer &rhs){
		value /= rhs.value;

		return *this;
	}

	/** Serialize Integer. Note that Integer is PseudoSerializable rather
	 *  than Serializable. This means that the Serializable interface is
	 *  implemented non-virtually.
	 *
	 *  @param mode Serialization mode.
	 *
	 *  @return Serialized string representation of the Integer. */
	std::string serialize(Serializable::Mode mode) const;
private:
	/** Value. */
	int value;
};

inline Integer::Integer(const std::string &serialization, Serializable::Mode mode){
	switch(mode){
	case Serializable::Mode::JSON:
		value = stoi(serialization);
		break;
	default:
		TBTKExit(
			"Integer::Integer()",
			"Only Serializable::Mode::JSON is supported yet.",
			""
		);
	}
}

inline std::string Integer::serialize(Serializable::Mode mode) const{
	switch(mode){
	case Serializable::Mode::JSON:
		return std::to_string(value);
	default:
		TBTKExit(
			"Integer::serialize()",
			"Only Serializable::Mode::JSON is supported yet.",
			""
		);
	}
}

};	//End of namespace TBTK

#endif