/*
    Qalculate (library)

    Copyright (C) 2003-2007, 2008, 2016, 2018  Hanna Knutsson (hanna.knutsson@protonmail.com)

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/

#include "support.h"

#include "BuiltinFunctions.h"
#include "util.h"
#include "MathStructure.h"
#include "Number.h"
#include "Calculator.h"
#include "Variable.h"
#include "Unit.h"

#include <sstream>
#include <time.h>
#include <limits>
#include <algorithm>

#if HAVE_UNORDERED_MAP
#	include <unordered_map>
	using std::unordered_map;
#elif 	defined(__GNUC__)

#	ifndef __has_include
#	define __has_include(x) 0
#	endif

#	if (defined(__clang__) && __has_include(<tr1/unordered_map>)) || (__GNUC__ >= 4 && __GNUC_MINOR__ >= 3)
#		include <tr1/unordered_map>
		namespace Sgi = std;
#		define unordered_map std::tr1::unordered_map
#	else
#		if __GNUC__ < 3
#			include <hash_map.h>
			namespace Sgi { using ::hash_map; }; // inherit globals
#		else
#			include <ext/hash_map>
#			if __GNUC__ == 3 && __GNUC_MINOR__ == 0
				namespace Sgi = std;               // GCC 3.0
#			else
				namespace Sgi = ::__gnu_cxx;       // GCC 3.1 and later
#			endif
#		endif
#		define unordered_map Sgi::hash_map
#	endif
#else      // ...  there are other compilers, right?
	namespace Sgi = std;
#	define unordered_map Sgi::hash_map
#endif

using std::string;
using std::cout;
using std::vector;
using std::ostream;
using std::endl;

#define FR_FUNCTION(FUNC)	Number nr(vargs[0].number()); if(!nr.FUNC() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !vargs[0].isApproximate()) || (!eo.allow_complex && nr.isComplex() && !vargs[0].number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !vargs[0].number().includesInfinity())) {return 0;} else {mstruct.set(nr); return 1;}
#define FR_FUNCTION_2(FUNC)	Number nr(vargs[0].number()); if(!nr.FUNC(vargs[1].number()) || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !vargs[0].isApproximate() && !vargs[1].isApproximate()) || (!eo.allow_complex && nr.isComplex() && !vargs[0].number().isComplex() && !vargs[1].number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !vargs[0].number().includesInfinity() && !vargs[1].number().includesInfinity())) {return 0;} else {mstruct.set(nr); return 1;}
#define FR_FUNCTION_2R(FUNC)	Number nr(vargs[1].number()); if(!nr.FUNC(vargs[0].number()) || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !vargs[0].isApproximate() && !vargs[1].isApproximate()) || (!eo.allow_complex && nr.isComplex() && !vargs[0].number().isComplex() && !vargs[1].number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !vargs[0].number().includesInfinity() && !vargs[1].number().includesInfinity())) {return 0;} else {mstruct.set(nr); return 1;}

#define REPRESENTS_FUNCTION(x, y) x::x() : MathFunction(#y, 1) {} int x::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {mstruct = vargs[0]; mstruct.eval(eo); if(mstruct.y()) {mstruct.clear(); mstruct.number().setTrue();} else {mstruct.clear(); mstruct.number().setFalse();} return 1;}

#define IS_NUMBER_FUNCTION(x, y) x::x() : MathFunction(#y, 1) {} int x::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {mstruct = vargs[0]; if(!mstruct.isNumber()) mstruct.eval(eo); if(mstruct.isNumber() && mstruct.number().y()) {mstruct.number().setTrue();} else {mstruct.clear(); mstruct.number().setFalse();} return 1;}

#define NON_COMPLEX_NUMBER_ARGUMENT(i)				NumberArgument *arg_non_complex##i = new NumberArgument(); arg_non_complex##i->setComplexAllowed(false); setArgumentDefinition(i, arg_non_complex##i);
#define NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(i)			NumberArgument *arg_non_complex##i = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, false); arg_non_complex##i->setComplexAllowed(false); setArgumentDefinition(i, arg_non_complex##i);
#define NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR_NONZERO(i)		NumberArgument *arg_non_complex##i = new NumberArgument("", ARGUMENT_MIN_MAX_NONZERO, true, false); arg_non_complex##i->setComplexAllowed(false); setArgumentDefinition(i, arg_non_complex##i);
#define RATIONAL_NUMBER_ARGUMENT_NO_ERROR(i)			NumberArgument *arg_rational##i = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, false); arg_rational##i->setRationalNumber(true); setArgumentDefinition(i, arg_rational##i);
#define RATIONAL_POLYNOMIAL_ARGUMENT(i)				Argument *arg_poly##i = new Argument(); arg_poly##i->setRationalPolynomial(true); setArgumentDefinition(i, arg_poly##i);
#define RATIONAL_POLYNOMIAL_ARGUMENT_HV(i)			Argument *arg_poly##i = new Argument(); arg_poly##i->setRationalPolynomial(true); arg_poly##i->setHandleVector(true); setArgumentDefinition(i, arg_poly##i);

extern string format_and_print(const MathStructure &mstruct);
extern bool replace_f_interval(MathStructure &mstruct, const EvaluationOptions &eo);
extern bool replace_intervals_f(MathStructure &mstruct);

bool calculate_arg(MathStructure &mstruct, const EvaluationOptions &eo);

VectorFunction::VectorFunction() : MathFunction("vector", -1) {
}
int VectorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs;
	mstruct.setType(STRUCT_VECTOR);
	return 1;
}
MatrixFunction::MatrixFunction() : MathFunction("matrix", 3) {
	Argument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE);
	arg->setHandleVector(false);
	setArgumentDefinition(1, arg);
	arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE);
	arg->setHandleVector(false);
	setArgumentDefinition(2, arg);
	setArgumentDefinition(3, new VectorArgument());
}
int MatrixFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	size_t rows = (size_t) vargs[0].number().uintValue();
	size_t columns = (size_t) vargs[1].number().uintValue();
	mstruct.clearMatrix(); mstruct.resizeMatrix(rows, columns, m_zero);
	size_t r = 1, c = 1;
	for(size_t i = 0; i < vargs[2].size(); i++) {
		if(r > rows || c > columns) {
			CALCULATOR->error(false, _("Too many elements (%s) for the dimensions (%sx%s) of the matrix."), i2s(vargs[2].size()).c_str(), i2s(rows).c_str(), i2s(columns).c_str(), NULL);
			break;
		}
		mstruct[r - 1][c - 1] = vargs[2][i];
		if(c == columns) {
			c = 1;
			r++;
		} else {
			c++;
		}
	}
	return 1;
}
RankFunction::RankFunction() : MathFunction("rank", 1, 2) {
	setArgumentDefinition(1, new VectorArgument(""));
	setArgumentDefinition(2, new BooleanArgument(""));
	setDefaultValue(2, "1");
}
int RankFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0];
	return mstruct.rankVector(vargs[1].number().getBoolean());
}
SortFunction::SortFunction() : MathFunction("sort", 1, 2) {
	setArgumentDefinition(1, new VectorArgument(""));
	setArgumentDefinition(2, new BooleanArgument(""));
	setDefaultValue(2, "1");
}
int SortFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0];
	return mstruct.sortVector(vargs[1].number().getBoolean());
}
MergeVectorsFunction::MergeVectorsFunction() : MathFunction("mergevectors", 1, -1) {
	setArgumentDefinition(1, new VectorArgument(""));
	setArgumentDefinition(2, new VectorArgument(""));
}
int MergeVectorsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct.clearVector();
	for(size_t i = 0; i < vargs.size(); i++) {
		if(CALCULATOR->aborted()) return 0;
		if(vargs[i].isVector()) {
			for(size_t i2 = 0; i2 < vargs[i].size(); i2++) {
				mstruct.addChild(vargs[i][i2]);
			}
		} else {
			mstruct.addChild(vargs[i]);
		}
	}
	return 1;
}
MatrixToVectorFunction::MatrixToVectorFunction() : MathFunction("matrix2vector", 1) {
	setArgumentDefinition(1, new MatrixArgument());
}
int MatrixToVectorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	vargs[0].matrixToVector(mstruct);
	return 1;
}
RowFunction::RowFunction() : MathFunction("row", 2) {
	setArgumentDefinition(1, new MatrixArgument());
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
}
int RowFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	size_t row = (size_t) vargs[1].number().uintValue();
	if(row > vargs[0].rows()) {
		CALCULATOR->error(true, _("Row %s does not exist in matrix."), format_and_print(vargs[1]).c_str(), NULL);
		return 0;
	}
	vargs[0].rowToVector(row, mstruct);
	return 1;
}
ColumnFunction::ColumnFunction() : MathFunction("column", 2) {
	setArgumentDefinition(1, new MatrixArgument());
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
}
int ColumnFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	size_t col = (size_t) vargs[1].number().uintValue();
	if(col > vargs[0].columns()) {
		CALCULATOR->error(true, _("Column %s does not exist in matrix."), format_and_print(vargs[1]).c_str(), NULL);
		return 0;
	}
	vargs[0].columnToVector(col, mstruct);
	return 1;
}
RowsFunction::RowsFunction() : MathFunction("rows", 1) {
	setArgumentDefinition(1, new VectorArgument(""));
}
int RowsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	if(!vargs[0].isMatrix()) mstruct = m_one;
	else mstruct = (int) vargs[0].rows();
	return 1;
}
ColumnsFunction::ColumnsFunction() : MathFunction("columns", 1) {
	setArgumentDefinition(1, new VectorArgument(""));
}
int ColumnsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	if(!vargs[0].isMatrix()) mstruct = (int) vargs[0].countChildren();
	else mstruct = (int) vargs[0].columns();
	return 1;
}
ElementsFunction::ElementsFunction() : MathFunction("elements", 1) {
	setArgumentDefinition(1, new VectorArgument(""));
}
int ElementsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	if(vargs[0].isMatrix()) {
		mstruct = (int) (vargs[0].rows() * vargs[0].columns());
	} else {
		mstruct = (int) vargs[0].countChildren();
	}
	return 1;
}
ElementFunction::ElementFunction() : MathFunction("element", 2, 3) {
	setArgumentDefinition(1, new VectorArgument(""));
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
	setArgumentDefinition(3, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SIZE));
	setDefaultValue(3, "0");
}
int ElementFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	if(vargs[2].number().isPositive() && vargs[0].isMatrix()) {
		size_t row = (size_t) vargs[1].number().uintValue();
		size_t col = (size_t) vargs[2].number().uintValue();
		bool b = true;
		if(col > vargs[0].columns()) {
			CALCULATOR->error(true, _("Column %s does not exist in matrix."), format_and_print(vargs[2]).c_str(), NULL);
			b = false;
		}
		if(row > vargs[0].rows()) {
			CALCULATOR->error(true, _("Row %s does not exist in matrix."), format_and_print(vargs[1]).c_str(), NULL);
			b = false;
		}
		if(b) {
			const MathStructure *em = vargs[0].getElement(row, col);
			if(em) mstruct = *em;
			else b = false;
		}
		return b;
	} else {
		if(vargs[2].number().isGreaterThan(1)) {
			CALCULATOR->error(false, _("Argument 3, %s, is ignored for vectors."), getArgumentDefinition(3)->name().c_str(), NULL);
		}
		size_t row = (size_t) vargs[1].number().uintValue();
		if(row > vargs[0].countChildren()) {
			CALCULATOR->error(true, _("Element %s does not exist in vector."), format_and_print(vargs[1]).c_str(), NULL);
			return 0;
		}
		mstruct = *vargs[0].getChild(row);
		return 1;
	}
}
DimensionFunction::DimensionFunction() : MathFunction("dimension", 1) {
	setArgumentDefinition(1, new VectorArgument(""));
}
int DimensionFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = (int) vargs[0].countChildren();
	return 1;
}
ComponentFunction::ComponentFunction() : MathFunction("component", 2) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
	setArgumentDefinition(2, new VectorArgument(""));
}
int ComponentFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	size_t i = (size_t) vargs[0].number().uintValue();
	if(i > vargs[1].countChildren()) {
		CALCULATOR->error(true, _("Element %s does not exist in vector."), format_and_print(vargs[0]).c_str(), NULL);
		return 0;
	}
	mstruct = *vargs[1].getChild(i);
	return 1;
}
LimitsFunction::LimitsFunction() : MathFunction("limits", 3) {
	setArgumentDefinition(1, new VectorArgument(""));
	Argument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SINT);
	arg->setHandleVector(false);
	setArgumentDefinition(2, arg);
	arg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SINT);
	arg->setHandleVector(false);
	setArgumentDefinition(3, arg);
}
int LimitsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	vargs[0].getRange(vargs[1].number().intValue(), vargs[2].number().intValue(), mstruct);
	return 1;
}
AreaFunction::AreaFunction() : MathFunction("area", 5) {
	setArgumentDefinition(1, new MatrixArgument(""));
	Argument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE);
	arg->setHandleVector(false);
	setArgumentDefinition(2, arg);
	arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE);
	arg->setHandleVector(false);
	setArgumentDefinition(3, arg);
	arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE);
	arg->setHandleVector(false);
	setArgumentDefinition(4, arg);
	arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE);
	arg->setHandleVector(false);
	setArgumentDefinition(5, arg);
}
int AreaFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	vargs[0].getArea(vargs[1].number().uintValue(), vargs[2].number().uintValue(), vargs[3].number().uintValue(), vargs[4].number().uintValue(), mstruct);
	return 1;
}
TransposeFunction::TransposeFunction() : MathFunction("transpose", 1) {
	setArgumentDefinition(1, new MatrixArgument());
}
int TransposeFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0];
	return mstruct.transposeMatrix();
}
IdentityFunction::IdentityFunction() : MathFunction("identity", 1) {
	ArgumentSet *arg = new ArgumentSet();
	arg->addArgument(new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
	MatrixArgument *marg = new MatrixArgument();
	marg->setSquareDemanded(true);
	arg->addArgument(marg);
	setArgumentDefinition(1, arg);
}
int IdentityFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	if(vargs[0].isMatrix()) {
		if(vargs[0].rows() != vargs[0].columns()) {
			return 0;
		}
		mstruct.setToIdentityMatrix(vargs[0].size());
	} else {
		mstruct.setToIdentityMatrix((size_t) vargs[0].number().uintValue());
	}
	return 1;
}
DeterminantFunction::DeterminantFunction() : MathFunction("det", 1) {
	MatrixArgument *marg = new MatrixArgument();
	marg->setSquareDemanded(true);
	setArgumentDefinition(1, marg);
}
int DeterminantFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	vargs[0].determinant(mstruct, eo);
	return !mstruct.isUndefined();
}
PermanentFunction::PermanentFunction() : MathFunction("permanent", 1) {
	MatrixArgument *marg = new MatrixArgument();
	marg->setSquareDemanded(true);
	setArgumentDefinition(1, marg);
}
int PermanentFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	vargs[0].permanent(mstruct, eo);
	return !mstruct.isUndefined();
}
CofactorFunction::CofactorFunction() : MathFunction("cofactor", 3) {
	setArgumentDefinition(1, new MatrixArgument());
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
	setArgumentDefinition(3, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
}
int CofactorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	vargs[0].cofactor((size_t) vargs[1].number().uintValue(), (size_t) vargs[2].number().uintValue(), mstruct, eo);
	return !mstruct.isUndefined();
}
AdjointFunction::AdjointFunction() : MathFunction("adj", 1) {
	MatrixArgument *marg = new MatrixArgument();
	marg->setSquareDemanded(true);
	setArgumentDefinition(1, marg);
}
int AdjointFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	if(!mstruct.adjointMatrix(eo)) return 0;
	return !mstruct.isUndefined();
}
InverseFunction::InverseFunction() : MathFunction("inverse", 1) {
	MatrixArgument *marg = new MatrixArgument();
	marg->setSquareDemanded(true);
	setArgumentDefinition(1, marg);
}
int InverseFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	return mstruct.invertMatrix(eo);
}
MagnitudeFunction::MagnitudeFunction() : MathFunction("magnitude", 1) {
}
int MagnitudeFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	if(mstruct.isVector()) {
		mstruct ^= nr_two;
		mstruct.raise(nr_half);
		return 1;
	} else if(mstruct.representsScalar()) {
		mstruct.transform(CALCULATOR->f_abs);
		return 1;
	}
	mstruct.eval(eo);
	if(mstruct.isVector()) {
		mstruct ^= nr_two;
		mstruct.raise(nr_half);
		return 1;
	}
	mstruct = vargs[0];
	mstruct.transform(CALCULATOR->f_abs);
	return 1;
}
HadamardFunction::HadamardFunction() : MathFunction("hadamard", 1, -1) {
	setArgumentDefinition(1, new VectorArgument());
	setArgumentDefinition(2, new VectorArgument());
}
int HadamardFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	bool b_matrix = vargs[0].isMatrix();
	for(size_t i3 = 1; i3 < vargs.size(); i3++) {
		if(b_matrix) {
			if(!vargs[i3].isMatrix() || vargs[i3].columns() != vargs[0].columns() || vargs[i3].rows() != vargs[0].rows()) {
				CALCULATOR->error(true, _("%s() requires that all matrices/vectors have the same dimensions."), preferredDisplayName().name.c_str(), NULL);
				return 0;
			}
		} else if(vargs[i3].size() != vargs[0].size()) {
			CALCULATOR->error(true, _("%s() requires that all matrices/vectors have the same dimensions."), preferredDisplayName().name.c_str(), NULL);
			return 0;
		}
	}
	mstruct = vargs[0];
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(b_matrix) {
			for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
				for(size_t i3 = 1; i3 < vargs.size(); i3++) mstruct[i][i2] *= vargs[i3][i][i2];
			}
		} else {
			for(size_t i3 = 1; i3 < vargs.size(); i3++) mstruct[i] *= vargs[i3][i];
		}
	}
	return 1;
}
EntrywiseFunction::EntrywiseFunction() : MathFunction("entrywise", 2) {
	VectorArgument *arg = new VectorArgument();
	arg->addArgument(new VectorArgument());
	arg->addArgument(new SymbolicArgument());
	arg->setReoccuringArguments(true);
	setArgumentDefinition(2, arg);
}
int EntrywiseFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[1].size() == 0) {
		mstruct = vargs[0];
		return 1;
	}
	bool b_matrix = vargs[1][0].isMatrix();
	for(size_t i3 = 2; i3 < vargs[1].size(); i3 += 2) {
		if(b_matrix) {
			if(!vargs[1][i3].isMatrix() || vargs[1][i3].columns() != vargs[1][0].columns() || vargs[1][i3].rows() != vargs[1][0].rows()) {
				CALCULATOR->error(true, _("%s() requires that all matrices/vectors have the same dimensions."), preferredDisplayName().name.c_str(), NULL);
				return 0;
			}
		} else if(vargs[1][i3].size() != vargs[1][0].size()) {
			CALCULATOR->error(true, _("%s() requires that all matrices/vectors have the same dimensions."), preferredDisplayName().name.c_str(), NULL);
			return 0;
		}
	}
	MathStructure mexpr(vargs[0]);
	EvaluationOptions eo2 = eo;
	eo2.calculate_functions = false;
	mexpr.eval(eo2);
	mstruct = vargs[1][0];
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(b_matrix) {
			for(size_t i2 = 0; i2 < mstruct[i].size(); i2++) {
				mstruct[i][i2] = mexpr;
				for(size_t i3 = 1; i3 < vargs[1].size(); i3 += 2) {
					mstruct[i][i2].replace(vargs[1][i3], vargs[1][i3 - 1][i][i2]);
				}
			}
		} else {
			mstruct[i] = mexpr;
			for(size_t i3 = 1; i3 < vargs[1].size(); i3 += 2) {
				mstruct[i].replace(vargs[1][i3], vargs[1][i3 - 1][i]);
			}
		}
	}
	return 1;
}

ZetaFunction::ZetaFunction() : MathFunction("zeta", 1, 1, SIGN_ZETA) {
	IntegerArgument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG);
	setArgumentDefinition(1, arg);
}
int ZetaFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].number().isZero()) {
		mstruct.set(1, 2, 0);
		return 1;
	} else if(vargs[0].number().isMinusOne()) {
		mstruct.set(1, 12, 0);
		return 1;
	} else if(vargs[0].number().isNegative() && vargs[0].number().isEven()) {
		mstruct.clear();
		return 1;
	} else if(vargs[0].number() == 2) {
		mstruct.set(CALCULATOR->v_pi);
		mstruct.raise(2);
		mstruct.divide(6);
		mstruct.mergePrecision(vargs[0]);
		return 1;
	} else if(vargs[0].number() == 4) {
		mstruct.set(CALCULATOR->v_pi);
		mstruct.raise(4);
		mstruct.divide(90);
		mstruct.mergePrecision(vargs[0]);
		return 1;
	} else if(vargs[0].number() == 6) {
		mstruct.set(CALCULATOR->v_pi);
		mstruct.raise(6);
		mstruct.divide(945);
		mstruct.mergePrecision(vargs[0]);
		return 1;
	} else if(vargs[0].number() == 8) {
		mstruct.set(CALCULATOR->v_pi);
		mstruct.raise(8);
		mstruct.divide(9450);
		mstruct.mergePrecision(vargs[0]);
		return 1;
	} else if(vargs[0].number() == 10) {
		mstruct.set(CALCULATOR->v_pi);
		mstruct.raise(10);
		mstruct.divide(93555);
		mstruct.mergePrecision(vargs[0]);
		return 1;
	}
	FR_FUNCTION(zeta)
}
GammaFunction::GammaFunction() : MathFunction("gamma", 1, 1, SIGN_CAPITAL_GAMMA) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1);
}
int GammaFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].number().isRational() && (eo.approximation == APPROXIMATION_EXACT || (eo.approximation == APPROXIMATION_TRY_EXACT && vargs[0].number().isLessThan(1000)))) {
		if(vargs[0].number().isInteger() && vargs[0].number().isPositive()) {
			mstruct.set(CALCULATOR->f_factorial, &vargs[0], NULL);
			mstruct[0] -= 1;
			return 1;
		} else if(vargs[0].number().denominatorIsTwo()) {
			Number nr(vargs[0].number());
			nr.floor();
			if(nr.isZero()) {
				MathStructure mtmp(CALCULATOR->v_pi);
				mstruct.set(CALCULATOR->f_sqrt, &mtmp, NULL);
				return 1;
			} else if(nr.isPositive()) {
				Number nr2(nr);
				nr2 *= 2;
				nr2 -= 1;
				nr2.doubleFactorial();
				Number nr3(2, 1, 0);
				nr3 ^= nr;
				nr2 /= nr3;
				mstruct = nr2;
				MathStructure mtmp1(CALCULATOR->v_pi);
				MathStructure mtmp2(CALCULATOR->f_sqrt, &mtmp1, NULL);
				mstruct *= mtmp2;
				return 1;
			} else {
				nr.negate();
				Number nr2(nr);
				nr2 *= 2;
				nr2 -= 1;
				nr2.doubleFactorial();
				Number nr3(2, 1, 0);
				nr3 ^= nr;
				if(nr.isOdd()) nr3.negate();
				nr3 /= nr2;
				mstruct = nr3;
				MathStructure mtmp1(CALCULATOR->v_pi);
				MathStructure mtmp2(CALCULATOR->f_sqrt, &mtmp1, NULL);
				mstruct *= mtmp2;
				return 1;
			}
		}
	}
	FR_FUNCTION(gamma)
}
DigammaFunction::DigammaFunction() : MathFunction("digamma", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1);
}
int DigammaFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].number().isOne()) {
		mstruct.set(CALCULATOR->v_euler);
		mstruct.negate();
		return 1;
	}
	FR_FUNCTION(digamma)
}
BetaFunction::BetaFunction() : MathFunction("beta", 2, 2, SIGN_CAPITAL_BETA) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false));
	setArgumentDefinition(2, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false));
}
int BetaFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0];
	mstruct.set(CALCULATOR->f_gamma, &vargs[0], NULL);
	MathStructure mstruct2(CALCULATOR->f_gamma, &vargs[1], NULL);
	mstruct *= mstruct2;
	mstruct2[0] += vargs[0];
	mstruct /= mstruct2;
	return 1;
}
AiryFunction::AiryFunction() : MathFunction("airy", 1) {
	NumberArgument *arg = new NumberArgument();
	Number fr(-500, 1, 0);
	arg->setMin(&fr);
	fr.set(500, 1, 0);
	arg->setMax(&fr);
	setArgumentDefinition(1, arg);
}
int AiryFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(airy)
}
BesseljFunction::BesseljFunction() : MathFunction("besselj", 2) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG));
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(2);
}
int BesseljFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION_2R(besselj)
}
BesselyFunction::BesselyFunction() : MathFunction("bessely", 2) {
	IntegerArgument *iarg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG);
	Number nmax(1000);
	iarg->setMax(&nmax);
	setArgumentDefinition(1, iarg);
	NON_COMPLEX_NUMBER_ARGUMENT(2);
}
int BesselyFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION_2R(bessely)
}
ErfFunction::ErfFunction() : MathFunction("erf", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1);
}
int ErfFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(erf)
}
ErfcFunction::ErfcFunction() : MathFunction("erfc", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1);
}
int ErfcFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(erfc)
}

FactorialFunction::FactorialFunction() : MathFunction("factorial", 1) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_NONNEGATIVE, true, false, INTEGER_TYPE_SLONG));
}
int FactorialFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(factorial)
}
bool FactorialFunction::representsPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool FactorialFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool FactorialFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool FactorialFunction::representsNonPositive(const MathStructure&, bool) const {return false;}
bool FactorialFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool FactorialFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool FactorialFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool FactorialFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool FactorialFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool FactorialFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool FactorialFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool FactorialFunction::representsEven(const MathStructure&, bool) const {return false;}
bool FactorialFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool FactorialFunction::representsUndefined(const MathStructure&) const {return false;}

DoubleFactorialFunction::DoubleFactorialFunction() : MathFunction("factorial2", 1) {
	IntegerArgument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG);
	Number nr(-1, 1, 0);
	arg->setMin(&nr);
	setArgumentDefinition(1, arg);
}
int DoubleFactorialFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(doubleFactorial)
}
bool DoubleFactorialFunction::representsPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool DoubleFactorialFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool DoubleFactorialFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool DoubleFactorialFunction::representsNonPositive(const MathStructure&, bool) const {return false;}
bool DoubleFactorialFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool DoubleFactorialFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool DoubleFactorialFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool DoubleFactorialFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool DoubleFactorialFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool DoubleFactorialFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool DoubleFactorialFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool DoubleFactorialFunction::representsEven(const MathStructure&, bool) const {return false;}
bool DoubleFactorialFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool DoubleFactorialFunction::representsUndefined(const MathStructure&) const {return false;}

MultiFactorialFunction::MultiFactorialFunction() : MathFunction("multifactorial", 2) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_NONNEGATIVE, true, true, INTEGER_TYPE_SLONG));
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SLONG));
}
int MultiFactorialFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION_2(multiFactorial)
}
bool MultiFactorialFunction::representsPositive(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool MultiFactorialFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool MultiFactorialFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool MultiFactorialFunction::representsNonPositive(const MathStructure&, bool) const {return false;}
bool MultiFactorialFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool MultiFactorialFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool MultiFactorialFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool MultiFactorialFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool MultiFactorialFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool MultiFactorialFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool MultiFactorialFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsInteger() && vargs[0].representsNonNegative();}
bool MultiFactorialFunction::representsEven(const MathStructure&, bool) const {return false;}
bool MultiFactorialFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool MultiFactorialFunction::representsUndefined(const MathStructure&) const {return false;}

BinomialFunction::BinomialFunction() : MathFunction("binomial", 2) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true));
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_ULONG));
}
int BinomialFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	Number nr;
	if(!nr.binomial(vargs[0].number(), vargs[1].number())) return 0;
	mstruct = nr;
	return 1;
}

BitXorFunction::BitXorFunction() : MathFunction("xor", 2) {
	ArgumentSet *arg = new ArgumentSet();
	arg->addArgument(new IntegerArgument("", ARGUMENT_MIN_MAX_NONE));
	arg->addArgument(new VectorArgument);
	setArgumentDefinition(1, arg);
	arg = new ArgumentSet();
	arg->addArgument(new IntegerArgument("", ARGUMENT_MIN_MAX_NONE));
	arg->addArgument(new VectorArgument);
	setArgumentDefinition(2, arg);
}
int BitXorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isNumber() && vargs[1].isNumber()) {
		Number nr(vargs[0].number());
		if(nr.bitXor(vargs[1].number()) && (eo.approximation >= APPROXIMATION_APPROXIMATE || !nr.isApproximate() || vargs[0].number().isApproximate() || vargs[1].number().isApproximate()) && (eo.allow_complex || !nr.isComplex() || vargs[0].number().isComplex() || vargs[1].number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || vargs[0].number().includesInfinity() || vargs[1].number().includesInfinity())) {
			mstruct.set(nr, true);
			return 1;
		}
	} else if(vargs[0].isVector() && vargs[1].isVector()) {
		int i1 = 0, i2 = 1;
		if(vargs[0].size() < vargs[1].size()) {
			i1 = 1;
			i2 = 0;
		}
		mstruct.clearVector();
		mstruct.resizeVector(vargs[i1].size(), m_undefined);
		size_t i = 0;
		for(; i < vargs[i2].size(); i++) {
			mstruct[i].set(CALCULATOR->f_xor, &vargs[i1][i], &vargs[i2][0], NULL);
		}
		for(; i < vargs[i1].size(); i++) {
			mstruct[i] = vargs[i1][i];
			mstruct[i].add(m_zero, OPERATION_GREATER);
		}
		return 1;
	}
	mstruct = vargs[0];
	mstruct.add(vargs[1], OPERATION_BITWISE_XOR);
	return 0;
}
XorFunction::XorFunction() : MathFunction("lxor", 2) {
}
int XorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	int b0, b1;
	if(vargs[0].representsNonPositive(true)) {
		b0 = 0;
	} else if(vargs[0].representsPositive(true)) {
		b0 = 1;
	} else {
		b0 = -1;
	}
	if(vargs[1].representsNonPositive(true)) {
		b1 = 0;
	} else if(vargs[1].representsPositive(true)) {
		b1 = 1;
	} else {
		b1 = -1;
	}
	if((b0 == 1 && b1 == 0) || (b0 == 0 && b1 == 1)) {
		mstruct = m_one;
		return 1;
	} else if(b0 >= 0 && b1 >= 0) {
		return 1;
	} else if(b0 == 0) {
		mstruct = vargs[1];
		mstruct.add(m_zero, OPERATION_GREATER);
		return 1;
	} else if(b0 == 1) {
		mstruct = vargs[1];
		mstruct.add(m_zero, OPERATION_EQUALS_LESS);
		return 1;
	} else if(b1 == 0) {
		mstruct = vargs[0];
		mstruct.add(m_zero, OPERATION_GREATER);
		return 1;
	} else if(b1 == 1) {
		mstruct = vargs[0];
		mstruct.add(m_zero, OPERATION_EQUALS_LESS);
		return 1;
	}
	mstruct = vargs[1];
	mstruct.setLogicalNot();
	mstruct.add(vargs[0], OPERATION_LOGICAL_AND);
	MathStructure mstruct2(vargs[0]);
	mstruct2.setLogicalNot();
	mstruct2.add(vargs[1], OPERATION_LOGICAL_AND);
	mstruct.add(mstruct2, OPERATION_LOGICAL_OR);
	return 1;
}

OddFunction::OddFunction() : MathFunction("odd", 1) {
	Argument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int OddFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	if(vargs[0].representsOdd()) {
		mstruct.set(1, 1, 0);
		return 1;
	} else if(vargs[0].representsEven()) {
		mstruct.clear();
		return 1;
	}
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.representsOdd()) {
		mstruct.set(1, 1, 0);
		return 1;
	} else if(mstruct.representsEven()) {
		mstruct.clear();
		return 1;
	}
	return -1;
}
EvenFunction::EvenFunction() : MathFunction("even", 1) {
	Argument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int EvenFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	if(vargs[0].representsEven()) {
		mstruct.set(1, 1, 0);
		return 1;
	} else if(vargs[0].representsOdd()) {
		mstruct.clear();
		return 1;
	}
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.representsEven()) {
		mstruct.set(1, 1, 0);
		return 1;
	} else if(mstruct.representsOdd()) {
		mstruct.clear();
		return 1;
	}
	return -1;
}
ShiftFunction::ShiftFunction() : MathFunction("shift", 2, 3) {
	setArgumentDefinition(1, new IntegerArgument());
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG));
	setArgumentDefinition(3, new BooleanArgument());
	setDefaultValue(3, "1");
}
int ShiftFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs.size() >= 3 && !vargs[2].number().getBoolean() && vargs[1].number().isNegative()) {
		Number nr(vargs[0].number());
		Number nr_div(vargs[1].number());
		if(!nr_div.negate() || !nr_div.exp2() || !nr.divide(nr_div) || !nr.trunc()) return false;
		mstruct.set(nr);
		return 1;
	}
	FR_FUNCTION_2(shift)
}
CircularShiftFunction::CircularShiftFunction() : MathFunction("bitrot", 2, 4) {
	setArgumentDefinition(1, new IntegerArgument());
	setArgumentDefinition(2, new IntegerArgument());
	setArgumentDefinition(3, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_UINT));
	setArgumentDefinition(4, new BooleanArgument());
	setDefaultValue(3, "0");
	setDefaultValue(4, "1");
}
int CircularShiftFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].number().isZero()) {
		mstruct.clear();
		return 1;
	}
	Number nr(vargs[0].number());
	unsigned int bits = vargs[2].number().uintValue();
	if(bits == 0) {
		bits = nr.integerLength();
		if(bits <= 8) bits = 8;
		else if(bits <= 16) bits = 16;
		else if(bits <= 32) bits = 32;
		else if(bits <= 64) bits = 64;
		else if(bits <= 128) bits = 128;
		else {
			bits = (unsigned int) ::ceil(::log2(bits));
			bits = ::pow(2, bits);
		}
	}
	Number nr_n(vargs[1].number());
	nr_n.rem(bits);
	if(nr_n.isZero()) {
		mstruct = nr;
		return 1;
	}
	PrintOptions po;
	po.base = BASE_BINARY;
	po.base_display = BASE_DISPLAY_NORMAL;
	po.binary_bits = bits;
	string str = nr.print(po);
	remove_blanks(str);
	if(str.length() < bits) return 0;
	if(nr_n.isNegative()) {
		nr_n.negate();
		std::rotate(str.rbegin(), str.rbegin() + nr_n.uintValue(), str.rend());
	} else {
		std::rotate(str.begin(), str.begin() + nr_n.uintValue(), str.end());
	}
	ParseOptions pao;
	pao.base = BASE_BINARY;
	pao.twos_complement = vargs[3].number().getBoolean();
	mstruct = Number(str, pao);
	return 1;
}
BitCmpFunction::BitCmpFunction() : MathFunction("bitcmp", 1, 3) {
	setArgumentDefinition(1, new IntegerArgument());
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_UINT));
	setDefaultValue(2, "0");
	setArgumentDefinition(3, new BooleanArgument());
	setDefaultValue(3, "0");
}
int BitCmpFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	Number nr(vargs[0].number());
	if(vargs.size() >= 3 && vargs[2].number().getBoolean()) {
		if(nr.bitNot()) {
			mstruct = nr;
			return 1;
		}
		return 0;
	}
	unsigned int bits = vargs[1].number().uintValue();
	if(bits == 0) {
		bits = nr.integerLength();
		if(bits <= 8) bits = 8;
		else if(bits <= 16) bits = 16;
		else if(bits <= 32) bits = 32;
		else if(bits <= 64) bits = 64;
		else if(bits <= 128) bits = 128;
		else {
			bits = (unsigned int) ::ceil(::log2(bits));
			bits = ::pow(2, bits);
		}
	}
	if(nr.bitCmp(bits)) {
		mstruct = nr;
		return 1;
	}
	return 0;
}

bool test_eval(MathStructure &mtest, const EvaluationOptions &eo) {
	EvaluationOptions eo2 = eo;
	eo2.assume_denominators_nonzero = false;
	eo2.approximation = APPROXIMATION_APPROXIMATE;
	CALCULATOR->beginTemporaryEnableIntervalArithmetic();
	CALCULATOR->beginTemporaryStopMessages();
	mtest.calculateFunctions(eo2);
	mtest.calculatesub(eo2, eo2, true);
	CALCULATOR->endTemporaryEnableIntervalArithmetic();
	if(CALCULATOR->endTemporaryStopMessages()) return false;
	return true;
}
bool has_interval_unknowns(MathStructure &m) {
	if(m.isVariable() && !m.variable()->isKnown()) {
		Assumptions *ass = ((UnknownVariable*) m.variable())->assumptions();
		return !((UnknownVariable*) m.variable())->interval().isUndefined() || (ass && ((ass->sign() != ASSUMPTION_SIGN_UNKNOWN && ass->sign() != ASSUMPTION_SIGN_NONZERO) || ass->min() || ass->max()));
	}
	for(size_t i = 0; i < m.size(); i++) {
		if(has_interval_unknowns(m[i])) return true;
	}
	return false;
}

AbsFunction::AbsFunction() : MathFunction("abs", 1) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
bool AbsFunction::representsPositive(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units) && vargs[0].representsNonZero(allow_units);}
bool AbsFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool AbsFunction::representsNonNegative(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool AbsFunction::representsNonPositive(const MathStructure&, bool) const {return false;}
bool AbsFunction::representsInteger(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsInteger(allow_units);}
bool AbsFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool AbsFunction::representsRational(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsRational(allow_units);}
bool AbsFunction::representsReal(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool AbsFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool AbsFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool AbsFunction::representsNonZero(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units) && vargs[0].representsNonZero(allow_units);}
bool AbsFunction::representsEven(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsEven(allow_units);}
bool AbsFunction::representsOdd(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsOdd(allow_units);}
bool AbsFunction::representsUndefined(const MathStructure &vargs) const {return vargs.size() == 1 && vargs[0].representsUndefined();}
int AbsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isNumber()) {
		if(eo.approximation != APPROXIMATION_APPROXIMATE && mstruct.number().hasImaginaryPart() && mstruct.number().hasRealPart()) {
			MathStructure m_i(mstruct.number().imaginaryPart());
			m_i ^= nr_two;
			mstruct.number().clearImaginary();
			mstruct.numberUpdated();
			mstruct ^= nr_two;
			mstruct += m_i;
			mstruct ^= nr_half;
			return 1;
		}
		Number nr(mstruct.number());
		if(!nr.abs() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate())) {
			return -1;
		}
		mstruct = nr;
		return 1;
	}
	if(mstruct.isPower() && mstruct[0].representsPositive()) {
		if(mstruct[1].isNumber() && !mstruct[1].number().hasRealPart()) {
			mstruct.set(1, 1, 0, true);
			return 1;
		} else if(mstruct[1].isMultiplication() && mstruct.size() > 0 && mstruct[1][0].isNumber() && !mstruct[1][0].number().hasRealPart()) {
			bool b = true;
			for(size_t i = 1; i < mstruct[1].size(); i++) {
				if(!mstruct[1][i].representsNonComplex()) {b = false; break;}
			}
			if(b) {
				mstruct.set(1, 1, 0, true);
				return 1;
			}
		}
	}
	if(mstruct.representsNegative(true)) {
		mstruct.negate();
		return 1;
	}
	if(mstruct.representsNonNegative(true)) {
		return 1;
	}
	if(mstruct.isMultiplication()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			mstruct[i].transform(STRUCT_FUNCTION);
			mstruct[i].setFunction(this);
		}
		mstruct.childrenUpdated();
		return 1;
	}
	if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_signum && mstruct.size() == 2) {
		mstruct[0].transform(this);
		mstruct.childUpdated(1);
		return 1;
	}
	if(mstruct.isPower() && mstruct[1].representsReal()) {
		mstruct[0].transform(this);
		return 1;
	}
	if(eo.approximation == APPROXIMATION_EXACT || has_interval_unknowns(mstruct)) {
		ComparisonResult cr = mstruct.compare(m_zero);
		if(COMPARISON_IS_EQUAL_OR_LESS(cr)) {
			return 1;
		} else if(COMPARISON_IS_EQUAL_OR_GREATER(cr)) {
			mstruct.negate();
			return 1;
		}
	}
	return -1;
}
GcdFunction::GcdFunction() : MathFunction("gcd", 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT_HV(1)
	RATIONAL_POLYNOMIAL_ARGUMENT_HV(2)
}
int GcdFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(MathStructure::gcd(vargs[0], vargs[1], mstruct, eo)) {
		return 1;
	}
	return 0;
}
LcmFunction::LcmFunction() : MathFunction("lcm", 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT_HV(1)
	RATIONAL_POLYNOMIAL_ARGUMENT_HV(2)
}
int LcmFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(MathStructure::lcm(vargs[0], vargs[1], mstruct, eo)) {
		return 1;
	}
	return 0;
}

HeavisideFunction::HeavisideFunction() : MathFunction("heaviside", 1) {
	NumberArgument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	arg->setComplexAllowed(false);
	setArgumentDefinition(1, arg);
}
bool HeavisideFunction::representsPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonNegative(true);}
bool HeavisideFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool HeavisideFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal(true);}
bool HeavisideFunction::representsNonPositive(const MathStructure&, bool) const {return false;}
bool HeavisideFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonZero() && vargs[0].representsReal(true);}
bool HeavisideFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal(true);}
bool HeavisideFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal(true);}
bool HeavisideFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex(true);}
bool HeavisideFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal(true);}
bool HeavisideFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool HeavisideFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonNegative(true);}
bool HeavisideFunction::representsEven(const MathStructure&, bool) const {return false;}
bool HeavisideFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool HeavisideFunction::representsUndefined(const MathStructure &vargs) const {return vargs.size() == 1 && vargs[0].representsUndefined();}
int HeavisideFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(!mstruct.representsNonComplex(true)) return false;
	if(mstruct.representsPositive(true)) {
		mstruct.set(1, 1, 0);
		return 1;
	}
	if(mstruct.representsNegative(true)) {
		mstruct.clear();
		return 1;
	}
	if(mstruct.isZero()) {
		mstruct = nr_half;
		return 1;
	}
	if(mstruct.isNumber() && mstruct.number().isInterval()) {
		if(!mstruct.number().isNonNegative()) {
			mstruct.number().setInterval(nr_half, nr_one);
		} else if(!mstruct.number().isNonPositive()) {
			mstruct.number().setInterval(nr_zero, nr_half);
		} else {
			mstruct.number().setInterval(nr_zero, nr_one);
		}
		return 1;
	}
	if(eo.approximation == APPROXIMATION_EXACT || has_interval_unknowns(mstruct)) {
		ComparisonResult cr = mstruct.compare(m_zero);
		if(cr == COMPARISON_RESULT_LESS) {
			mstruct.set(1, 1, 0);
			return 1;
		} else if(cr == COMPARISON_RESULT_GREATER) {
			mstruct.clear();
			return 1;
		}
	}
	return -1;
}
DiracFunction::DiracFunction() : MathFunction("dirac", 1) {
	NumberArgument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setComplexAllowed(false);
	setArgumentDefinition(1, arg);
}
bool DiracFunction::representsPositive(const MathStructure&, bool allow_units) const {return false;}
bool DiracFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsNonNegative(const MathStructure&, bool) const {return true;}
bool DiracFunction::representsNonPositive(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsInteger(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsNumber(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsRational(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex(true);}
bool DiracFunction::representsReal(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsNonZero(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsEven(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool DiracFunction::representsUndefined(const MathStructure &vargs) const {return vargs.size() == 1 && vargs[0].representsUndefined();}
int DiracFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(!mstruct.representsNonComplex(true)) return false;
	if(mstruct.representsNonZero(true)) {
		mstruct.clear();
		return 1;
	}
	if(mstruct.isZero()) {
		mstruct = nr_plus_inf;
		return 1;
	}
	if(mstruct.isNumber() && mstruct.number().isInterval() && !mstruct.number().isNonZero()) {
		mstruct.number().setInterval(nr_zero, nr_plus_inf);
		return 1;
	}
	if(eo.approximation == APPROXIMATION_EXACT || has_interval_unknowns(mstruct)) {
		ComparisonResult cr = mstruct.compare(m_zero);
		if(COMPARISON_IS_NOT_EQUAL(cr)) {
			mstruct.clear();
			return 1;
		}
	}
	return -1;
}
SignumFunction::SignumFunction() : MathFunction("sgn", 1, 2) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
	setDefaultValue(2, "0");
}
bool SignumFunction::representsPositive(const MathStructure&, bool allow_units) const {return false;}
bool SignumFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool SignumFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() >= 1 && vargs[0].representsNonNegative(true);}
bool SignumFunction::representsNonPositive(const MathStructure &vargs, bool) const {return vargs.size() >= 1 && vargs[0].representsNonPositive(true);}
bool SignumFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() >= 1 && vargs[0].representsReal(true);}
bool SignumFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() >= 1 && vargs[0].representsNumber(true);}
bool SignumFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() >= 1 && vargs[0].representsReal(true);}
bool SignumFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() >= 1 && vargs[0].representsNonComplex(true);}
bool SignumFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() >= 1 && vargs[0].representsReal(true);}
bool SignumFunction::representsComplex(const MathStructure &vargs, bool) const {return vargs.size() >= 1 && vargs[0].representsComplex(true);}
bool SignumFunction::representsNonZero(const MathStructure &vargs, bool) const {return (vargs.size() == 2 && !vargs[1].isZero()) || (vargs.size() >= 1 && vargs[0].representsNonZero(true));}
bool SignumFunction::representsEven(const MathStructure&, bool) const {return false;}
bool SignumFunction::representsOdd(const MathStructure &vargs, bool b) const {return representsNonZero(vargs, b);}
bool SignumFunction::representsUndefined(const MathStructure &vargs) const {return vargs.size() >= 1 && vargs[0].representsUndefined();}
int SignumFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isNumber() && (vargs.size() == 1 || vargs[1].isZero())) {
		Number nr(mstruct.number());
		if(!nr.signum() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate())) {
			if(mstruct.number().isNonZero()) {
				MathStructure *mabs = new MathStructure(mstruct);
				mabs->transform(STRUCT_FUNCTION);
				mabs->setFunction(CALCULATOR->f_abs);
				mstruct.divide_nocopy(mabs);
				return 1;
			}
			return -1;
		} else {
			mstruct = nr;
			return 1;
		}
	}
	if((vargs.size() > 1 && vargs[1].isOne() && mstruct.representsNonNegative(true)) || mstruct.representsPositive(true)) {
		mstruct.set(1, 1, 0);
		return 1;
	}
	if((vargs.size() > 1 && vargs[1].isMinusOne() && mstruct.representsNonPositive(true)) || mstruct.representsNegative(true)) {
		mstruct.set(-1, 1, 0);
		return 1;
	}
	if(mstruct.isMultiplication()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(vargs.size() > 1) mstruct[i].transform(STRUCT_FUNCTION, vargs[1]);
			else mstruct[i].transform(STRUCT_FUNCTION);
			mstruct[i].setFunction(this);

		}
		mstruct.childrenUpdated();
		return 1;
	}
	if(vargs.size() > 1 && mstruct.isZero()) {
		mstruct.set(vargs[1], true);
		return 1;
	}
	if(eo.approximation == APPROXIMATION_EXACT || has_interval_unknowns(mstruct)) {
		ComparisonResult cr = mstruct.compare(m_zero);
		if(cr == COMPARISON_RESULT_LESS || (vargs.size() > 1 && vargs[1].isOne() && COMPARISON_IS_EQUAL_OR_LESS(cr))) {
			mstruct.set(1, 1, 0);
			return 1;
		} else if(cr == COMPARISON_RESULT_GREATER || (vargs.size() > 1 && vargs[1].isMinusOne() && COMPARISON_IS_EQUAL_OR_GREATER(cr))) {
			mstruct.set(-1, 1, 0);
			return 1;
		}
	}
	return -1;
}

CeilFunction::CeilFunction() : MathFunction("ceil", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1)
}
int CeilFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(ceil)
}
bool CeilFunction::representsPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsPositive();}
bool CeilFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool CeilFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative();}
bool CeilFunction::representsNonPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonPositive();}
bool CeilFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool CeilFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool CeilFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool CeilFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool CeilFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool CeilFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool CeilFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsPositive();}
bool CeilFunction::representsEven(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsEven();}
bool CeilFunction::representsOdd(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsOdd();}
bool CeilFunction::representsUndefined(const MathStructure&) const {return false;}

FloorFunction::FloorFunction() : MathFunction("floor", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1)
}
int FloorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(floor)
}
bool FloorFunction::representsPositive(const MathStructure&, bool) const {return false;}
bool FloorFunction::representsNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNegative();}
bool FloorFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative();}
bool FloorFunction::representsNonPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonPositive();}
bool FloorFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool FloorFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool FloorFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool FloorFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool FloorFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool FloorFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool FloorFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNegative();}
bool FloorFunction::representsEven(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsEven();}
bool FloorFunction::representsOdd(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsOdd();}
bool FloorFunction::representsUndefined(const MathStructure&) const {return false;}

TruncFunction::TruncFunction() : MathFunction("trunc", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1)
}
int TruncFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(trunc)
}
bool TruncFunction::representsPositive(const MathStructure&, bool) const {return false;}
bool TruncFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool TruncFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative();}
bool TruncFunction::representsNonPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonPositive();}
bool TruncFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool TruncFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool TruncFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool TruncFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool TruncFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool TruncFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool TruncFunction::representsNonZero(const MathStructure&, bool) const {return false;}
bool TruncFunction::representsEven(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsEven();}
bool TruncFunction::representsOdd(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsOdd();}
bool TruncFunction::representsUndefined(const MathStructure&) const {return false;}

RoundFunction::RoundFunction() : MathFunction("round", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1)
}
int RoundFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(round)
}
bool RoundFunction::representsPositive(const MathStructure&, bool) const {return false;}
bool RoundFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool RoundFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative();}
bool RoundFunction::representsNonPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonPositive();}
bool RoundFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool RoundFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool RoundFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool RoundFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool RoundFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool RoundFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool RoundFunction::representsNonZero(const MathStructure&, bool) const {return false;}
bool RoundFunction::representsEven(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsEven();}
bool RoundFunction::representsOdd(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsInteger() && vargs[0].representsOdd();}
bool RoundFunction::representsUndefined(const MathStructure&) const {return false;}

FracFunction::FracFunction() : MathFunction("frac", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1)
}
int FracFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(frac)
}
IntFunction::IntFunction() : MathFunction("int", 1) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1)
}
int IntFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(trunc)
}
NumeratorFunction::NumeratorFunction() : MathFunction("numerator", 1) {
	NumberArgument *arg_rational_1 = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg_rational_1->setRationalNumber(true);
	arg_rational_1->setHandleVector(true);
	setArgumentDefinition(1, arg_rational_1);
}
int NumeratorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	if(vargs[0].isNumber()) {
		if(vargs[0].number().isInteger()) {
			mstruct = vargs[0];
			return 1;
		} else if(vargs[0].number().isRational()) {
			mstruct.set(vargs[0].number().numerator());
			return 1;
		}
		return 0;
	} else if(vargs[0].representsInteger()) {
		mstruct = vargs[0];
		return 1;
	}
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.representsInteger()) {
		return 1;
	} else if(mstruct.isNumber() && mstruct.number().isRational()) {
		Number nr(mstruct.number().numerator());
		mstruct.set(nr);
		return 1;
	}
	return -1;
}
DenominatorFunction::DenominatorFunction() : MathFunction("denominator", 1) {
	RATIONAL_NUMBER_ARGUMENT_NO_ERROR(1)
}
int DenominatorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct.set(vargs[0].number().denominator());
	return 1;
}
RemFunction::RemFunction() : MathFunction("rem", 2) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1)
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR_NONZERO(2)
}
int RemFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION_2(rem)
}
ModFunction::ModFunction() : MathFunction("mod", 2) {
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR(1)
	NON_COMPLEX_NUMBER_ARGUMENT_NO_ERROR_NONZERO(2)
}
int ModFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION_2(mod)
}

PolynomialUnitFunction::PolynomialUnitFunction() : MathFunction("punit", 1, 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT(1)
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
}
int PolynomialUnitFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct.set(vargs[0].polynomialUnit(vargs[1]), 0);
	return 1;
}
PolynomialPrimpartFunction::PolynomialPrimpartFunction() : MathFunction("primpart", 1, 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT(1)
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
}
int PolynomialPrimpartFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	vargs[0].polynomialPrimpart(vargs[1], mstruct, eo);
	return 1;
}
PolynomialContentFunction::PolynomialContentFunction() : MathFunction("pcontent", 1, 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT(1)
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
}
int PolynomialContentFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	vargs[0].polynomialContent(vargs[1], mstruct, eo);
	return 1;
}
CoeffFunction::CoeffFunction() : MathFunction("coeff", 2, 3) {
	RATIONAL_POLYNOMIAL_ARGUMENT(1)
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_NONNEGATIVE));
	setArgumentDefinition(3, new SymbolicArgument());
	setDefaultValue(3, "undefined");
}
int CoeffFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	vargs[0].coefficient(vargs[2], vargs[1].number(), mstruct);
	return 1;
}
LCoeffFunction::LCoeffFunction() : MathFunction("lcoeff", 1, 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT(1)
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
}
int LCoeffFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	vargs[0].lcoefficient(vargs[1], mstruct);
	return 1;
}
TCoeffFunction::TCoeffFunction() : MathFunction("tcoeff", 1, 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT(1)
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
}
int TCoeffFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	vargs[0].tcoefficient(vargs[1], mstruct);
	return 1;
}
DegreeFunction::DegreeFunction() : MathFunction("degree", 1, 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT(1)
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
}
int DegreeFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0].degree(vargs[1]);
	return 1;
}
LDegreeFunction::LDegreeFunction() : MathFunction("ldegree", 1, 2) {
	RATIONAL_POLYNOMIAL_ARGUMENT(1)
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
}
int LDegreeFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0].ldegree(vargs[1]);
	return 1;
}

ImFunction::ImFunction() : MathFunction("im", 1) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int ImFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isNumber()) {
		mstruct = mstruct.number().imaginaryPart();
		return 1;
	} else if(mstruct.representsReal(!eo.keep_zero_units)) {
		mstruct.clear(true);
		return 1;
	} else if(mstruct.isUnit_exp()) {
		mstruct *= m_zero;
		mstruct.swapChildren(1, 2);
		return 1;
	} else if(mstruct.isPower() && mstruct[1].isNumber() && mstruct[1].number().denominatorIsTwo() && mstruct[0].representsNegative()) {
		mstruct[0].negate();
		Number num = mstruct[1].number().numerator();
		num.rem(4);
		if(num == 3 || num == -1) mstruct.negate();
		return 1;
	} else if(mstruct.isMultiplication() && mstruct.size() > 0) {
		if(mstruct[0].isNumber()) {
			Number nr = mstruct[0].number();
			mstruct.delChild(1, true);
			if(nr.hasImaginaryPart()) {
				if(nr.hasRealPart()) {
					MathStructure *madd = new MathStructure(mstruct);
					mstruct.transform(CALCULATOR->f_re);
					madd->transform(this);
					madd->multiply(nr.realPart());
					mstruct.multiply(nr.imaginaryPart());
					mstruct.add_nocopy(madd);
					return 1;
				}
				mstruct.transform(CALCULATOR->f_re);
				mstruct.multiply(nr.imaginaryPart());
				return 1;
			}
			mstruct.transform(this);
			mstruct.multiply(nr.realPart());
			return 1;
		}
		MathStructure *mreal = NULL;
		for(size_t i = 0; i < mstruct.size();) {
			if(mstruct[i].representsReal(true)) {
				if(!mreal) {
					mreal = new MathStructure(mstruct[i]);
				} else {
					mstruct[i].ref();
					if(!mreal->isMultiplication()) mreal->transform(STRUCT_MULTIPLICATION);
					mreal->addChild_nocopy(&mstruct[i]);
				}
				mstruct.delChild(i + 1);
			} else {
				i++;
			}
		}
		if(mreal) {
			if(mstruct.size() == 0) mstruct.clear(true);
			else if(mstruct.size() == 1) mstruct.setToChild(1, true);
			mstruct.transform(this);
			mstruct.multiply_nocopy(mreal);
			return 1;
		}
	}
	return -1;
}
bool ImFunction::representsPositive(const MathStructure&, bool) const {return false;}
bool ImFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool ImFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool ImFunction::representsNonPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool ImFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool ImFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber();}
bool ImFunction::representsRational(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool ImFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber();}
bool ImFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool ImFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool ImFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsComplex();}
bool ImFunction::representsEven(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool ImFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool ImFunction::representsUndefined(const MathStructure&) const {return false;}

ReFunction::ReFunction() : MathFunction("re", 1) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int ReFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isNumber()) {
		mstruct = mstruct.number().realPart();
		return 1;
	} else if(mstruct.representsReal(true)) {
		return 1;
	} else if(mstruct.isPower() && mstruct[1].isNumber() && mstruct[1].number().denominatorIsTwo() && mstruct[0].representsNegative()) {
		mstruct.clear(true);
		return 1;
	} else if(mstruct.isMultiplication() && mstruct.size() > 0) {
		if(mstruct[0].isNumber()) {
			Number nr = mstruct[0].number();
			mstruct.delChild(1, true);
			if(nr.hasImaginaryPart()) {
				if(nr.hasRealPart()) {
					MathStructure *madd = new MathStructure(mstruct);
					mstruct.transform(CALCULATOR->f_im);
					madd->transform(this);
					madd->multiply(nr.realPart());
					mstruct.multiply(-nr.imaginaryPart());
					mstruct.add_nocopy(madd);
					return 1;
				}
				mstruct.transform(CALCULATOR->f_im);
				mstruct.multiply(-nr.imaginaryPart());
				return 1;
			}
			mstruct.transform(this);
			mstruct.multiply(nr.realPart());
			return 1;
		}
		MathStructure *mreal = NULL;
		for(size_t i = 0; i < mstruct.size();) {
			if(mstruct[i].representsReal(true)) {
				if(!mreal) {
					mreal = new MathStructure(mstruct[i]);
				} else {
					mstruct[i].ref();
					if(!mreal->isMultiplication()) mreal->transform(STRUCT_MULTIPLICATION);
					mreal->addChild_nocopy(&mstruct[i]);
				}
				mstruct.delChild(i + 1);
			} else {
				i++;
			}
		}
		if(mreal) {
			if(mstruct.size() == 0) mstruct.clear(true);
			else if(mstruct.size() == 1) mstruct.setToChild(1, true);
			mstruct.transform(this);
			mstruct.multiply_nocopy(mreal);
			return 1;
		}
	}
	return -1;
}
bool ReFunction::representsPositive(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsPositive(allow_units);}
bool ReFunction::representsNegative(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNegative(allow_units);}
bool ReFunction::representsNonNegative(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNonNegative(allow_units);}
bool ReFunction::representsNonPositive(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNonPositive(allow_units);}
bool ReFunction::representsInteger(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsInteger(allow_units);}
bool ReFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool ReFunction::representsRational(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsRational(allow_units);}
bool ReFunction::representsReal(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool ReFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
bool ReFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool ReFunction::representsNonZero(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsReal(allow_units) && vargs[0].representsNonZero(true);}
bool ReFunction::representsEven(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsEven(allow_units);}
bool ReFunction::representsOdd(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsOdd(allow_units);}
bool ReFunction::representsUndefined(const MathStructure&) const {return false;}

SqrtFunction::SqrtFunction() : MathFunction("sqrt", 1) {
	Argument *arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int SqrtFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	if(!vargs[0].representsScalar()) {
		mstruct.eval(eo);
		if(mstruct.isVector()) return -1;
	}
	mstruct.raise(nr_half);
	return 1;
}
bool SqrtFunction::representsPositive(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsPositive(allow_units);}
bool SqrtFunction::representsNegative(const MathStructure&, bool) const {return false;}
bool SqrtFunction::representsNonNegative(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNonNegative(allow_units);}
bool SqrtFunction::representsNonPositive(const MathStructure&, bool) const {return false;}
bool SqrtFunction::representsInteger(const MathStructure&, bool) const {return false;}
bool SqrtFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool SqrtFunction::representsRational(const MathStructure&, bool) const {return false;}
bool SqrtFunction::representsReal(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNonNegative(allow_units);}
bool SqrtFunction::representsNonComplex(const MathStructure &vargs, bool allow_units) const {return representsReal(vargs, allow_units);}
bool SqrtFunction::representsComplex(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNegative(allow_units);}
bool SqrtFunction::representsNonZero(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNonZero(allow_units);}
bool SqrtFunction::representsEven(const MathStructure&, bool) const {return false;}
bool SqrtFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool SqrtFunction::representsUndefined(const MathStructure&) const {return false;}
CbrtFunction::CbrtFunction() : MathFunction("cbrt", 1) {
	Argument *arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int CbrtFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	if(vargs[0].representsNegative(true)) {
		mstruct = vargs[0];
		mstruct.negate();
		mstruct.raise(Number(1, 3, 0));
		mstruct.negate();
	} else if(vargs[0].representsNonNegative(true)) {
		mstruct = vargs[0];
		mstruct.raise(Number(1, 3, 0));
	} else {
		MathStructure mroot(3, 1, 0);
		mstruct.set(CALCULATOR->f_root, &vargs[0], &mroot, NULL);
	}
	return 1;
}
RootFunction::RootFunction() : MathFunction("root", 2) {
	NumberArgument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setComplexAllowed(false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
	NumberArgument *arg2 = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, true);
	arg2->setComplexAllowed(false);
	arg2->setRationalNumber(true);
	arg2->setHandleVector(true);
	setArgumentDefinition(2, arg2);
}
int RootFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	if(vargs[1].number().isOne()) {
		mstruct = vargs[0];
		return 1;
	}
	if(!vargs[1].number().isInteger() || !vargs[1].number().isPositive()) {
		mstruct = vargs[0];
		if(!vargs[0].representsScalar()) {
			mstruct.eval(eo);
		}
		if(mstruct.isVector()) return -1;
		Number nr_root(vargs[1].number().numerator());
		nr_root.setPrecisionAndApproximateFrom(vargs[1].number());
		Number nr_pow(vargs[1].number().denominator());
		nr_pow.setPrecisionAndApproximateFrom(vargs[1].number());
		if(nr_root.isNegative()) {
			nr_root.negate();
			nr_pow.negate();
		}
		if(nr_root.isOne()) {
			mstruct ^= nr_pow;
		} else if(nr_root.isZero()) {
			mstruct ^= nr_zero;
		} else {
			mstruct ^= nr_pow;
			mstruct.transform(this);
			mstruct.addChild(nr_root);
		}
		return 1;
	}
	if(vargs[0].representsNonNegative(true)) {
		mstruct = vargs[0];
		Number nr_exp(vargs[1].number());
		nr_exp.recip();
		mstruct.raise(nr_exp);
		return 1;
	} else if(vargs[1].number().isOdd() && vargs[0].representsNegative(true)) {
		mstruct = vargs[0];
		mstruct.negate();
		Number nr_exp(vargs[1].number());
		nr_exp.recip();
		mstruct.raise(nr_exp);
		mstruct.negate();
		return 1;
	}
	bool eval_mstruct = !vargs[0].isNumber();
	Number nr;
	if(eval_mstruct) {
		mstruct = vargs[0];
		if(eo.approximation == APPROXIMATION_TRY_EXACT && mstruct.representsScalar()) {
			EvaluationOptions eo2 = eo;
			eo2.approximation = APPROXIMATION_EXACT;
			mstruct.eval(eo2);
		} else {
			mstruct.eval(eo);
		}
		if(mstruct.isVector()) {
			if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
			return -1;
		}
		if(mstruct.representsNonNegative(true)) {
			Number nr_exp(vargs[1].number());
			nr_exp.recip();
			mstruct.raise(nr_exp);
			return 1;
		} else if(vargs[1].number().isOdd() && mstruct.representsNegative(true)) {
			mstruct.negate();
			Number nr_exp(vargs[1].number());
			nr_exp.recip();
			mstruct.raise(nr_exp);
			mstruct.negate();
			return 1;
		}
		if(!mstruct.isNumber()) {
			if(mstruct.isPower() && mstruct[1].isNumber() && mstruct[1].number().isInteger()) {
				if(mstruct[1] == vargs[1]) {
					if(mstruct[1].number().isEven()) {
						if(!mstruct[0].representsReal(true)) return -1;
						mstruct.delChild(2);
						mstruct.setType(STRUCT_FUNCTION);
						mstruct.setFunction(CALCULATOR->f_abs);
					} else {
						mstruct.setToChild(1);
					}
					return 1;
				} else if(mstruct[1].number().isIntegerDivisible(vargs[1].number())) {
					if(mstruct[1].number().isEven()) {
						if(!mstruct[0].representsReal(true)) return -1;
						mstruct[0].transform(STRUCT_FUNCTION);
						mstruct[0].setFunction(CALCULATOR->f_abs);
					}
					mstruct[1].divide(vargs[1].number());
					return 1;
				} else if(!mstruct[1].number().isMinusOne() && vargs[1].number().isIntegerDivisible(mstruct[1].number())) {
					if(mstruct[1].number().isEven()) {
						if(!mstruct[0].representsReal(true)) return -1;
						mstruct[0].transform(STRUCT_FUNCTION);
						mstruct[0].setFunction(CALCULATOR->f_abs);
					}
					Number new_root(vargs[1].number());
					new_root.divide(mstruct[1].number());
					bool bdiv = new_root.isNegative();
					if(bdiv) new_root.negate();
					mstruct[1] = new_root;
					mstruct.setType(STRUCT_FUNCTION);
					mstruct.setFunction(this);
					if(bdiv) mstruct.raise(nr_minus_one);
					return 1;
				} else if(mstruct[1].number().isMinusOne()) {
					mstruct[0].transform(STRUCT_FUNCTION, vargs[1]);
					mstruct[0].setFunction(this);
					return 1;
				} else if(mstruct[1].number().isNegative()) {
					mstruct[1].number().negate();
					mstruct.transform(STRUCT_FUNCTION, vargs[1]);
					mstruct.setFunction(this);
					mstruct.raise(nr_minus_one);
					return 1;
				}
			} else if(mstruct.isPower() && mstruct[1].isNumber() && mstruct[1].number().isRational() && mstruct[1].number().denominatorIsTwo()) {
				Number new_root(vargs[1].number());
				new_root.multiply(Number(2, 1, 0));
				if(mstruct[1].number().numeratorIsOne()) {
					mstruct[1].number().set(new_root, true);
					mstruct.setType(STRUCT_FUNCTION);
					mstruct.setFunction(this);
				} else {
					mstruct[1].number().set(mstruct[1].number().numerator(), true);
					mstruct.transform(STRUCT_FUNCTION);
					mstruct.addChild(new_root);
					mstruct.setFunction(this);
				}
				return 1;
			} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_sqrt && mstruct.size() == 1) {
				Number new_root(vargs[1].number());
				new_root.multiply(Number(2, 1, 0));
				mstruct.addChild(new_root);
				mstruct.setFunction(this);
				return 1;
			} else if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_root && mstruct.size() == 2 && mstruct[1].isNumber() && mstruct[1].number().isInteger() && mstruct[1].number().isPositive()) {
				Number new_root(vargs[1].number());
				new_root.multiply(mstruct[1].number());
				mstruct[1] = new_root;
				return 1;
			} else if(mstruct.isMultiplication()) {
				bool b = true;
				for(size_t i = 0; i < mstruct.size(); i++) {
					if(!mstruct[i].representsReal(true)) {
						b = false;
						break;
					}
				}
				if(b) {
					if(vargs[1].number().isOdd()) {
						bool b_neg = false;
						for(size_t i = 0; i < mstruct.size(); i++) {
							if(mstruct[i].isNumber() && mstruct[i].number().isNegative() && !mstruct[i].isMinusOne()) {
								mstruct[i].negate();
								b_neg = !b_neg;
							}
							mstruct[i].transform(STRUCT_FUNCTION, vargs[1]);
							mstruct[i].setFunction(this);
						}
						if(b_neg) mstruct.insertChild_nocopy(new MathStructure(-1, 1, 0), 1);
						return 1;
					} else {
						for(size_t i = 0; i < mstruct.size(); i++) {
							if(mstruct[i].isNumber() && mstruct[i].number().isNegative() && !mstruct[i].isMinusOne()) {
								MathStructure *mmul = new MathStructure(this, &mstruct[i], &vargs[1], NULL);
								(*mmul)[0].negate();
								mstruct[i] = nr_minus_one;
								mstruct.transform(STRUCT_FUNCTION, vargs[1]);
								mstruct.setFunction(this);
								mstruct.multiply_nocopy(mmul);
								return true;
							} else if(mstruct[i].representsPositive()) {
								mstruct[i].transform(STRUCT_FUNCTION, vargs[1]);
								mstruct[i].setFunction(this);
								mstruct[i].ref();
								MathStructure *mmul = &mstruct[i];
								mstruct.delChild(i + 1, true);
								mstruct.transform(STRUCT_FUNCTION, vargs[1]);
								mstruct.setFunction(this);
								mstruct.multiply_nocopy(mmul);
								return true;
							}
						}
					}
				}
			}
			return -1;
		}
		nr = mstruct.number();
	} else {
		nr = vargs[0].number();
	}
	if(!nr.root(vargs[1].number()) || (eo.approximation < APPROXIMATION_APPROXIMATE && nr.isApproximate() && !vargs[0].isApproximate() && !mstruct.isApproximate() && !vargs[1].isApproximate()) || (!eo.allow_complex && nr.isComplex() && !vargs[0].number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !vargs[0].number().includesInfinity())) {
		if(!eval_mstruct) {
			if(vargs[0].number().isNegative() && vargs[1].number().isOdd()) {
				mstruct.set(this, &vargs[0], &vargs[1], NULL);
				mstruct[0].number().negate();
				mstruct.negate();
				return 1;
			}
			return 0;
		} else if(mstruct.number().isNegative() && vargs[1].number().isOdd()) {
			mstruct.number().negate();
			mstruct.transform(STRUCT_FUNCTION, vargs[1]);
			mstruct.setFunction(this);
			mstruct.negate();
			return 1;
		}
	} else {
		mstruct.set(nr);
		return 1;
	}
	return -1;
}
bool RootFunction::representsPositive(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsPositive(allow_units);}
bool RootFunction::representsNegative(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsOdd() && vargs[1].representsPositive() && vargs[0].representsNegative(allow_units);}
bool RootFunction::representsNonNegative(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsNonNegative(allow_units);}
bool RootFunction::representsNonPositive(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsOdd() && vargs[1].representsPositive() && vargs[0].representsNonPositive(allow_units);}
bool RootFunction::representsInteger(const MathStructure&, bool) const {return false;}
bool RootFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsNumber(allow_units);}
bool RootFunction::representsRational(const MathStructure&, bool) const {return false;}
bool RootFunction::representsReal(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsReal(allow_units) && (vargs[0].representsNonNegative(allow_units) || vargs[1].representsOdd());}
bool RootFunction::representsNonComplex(const MathStructure &vargs, bool allow_units) const {return representsReal(vargs, allow_units);}
bool RootFunction::representsComplex(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && (vargs[0].representsComplex(allow_units) || (vargs[1].representsEven() && vargs[0].representsNegative(allow_units)));}
bool RootFunction::representsNonZero(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsInteger() && vargs[1].representsPositive() && vargs[0].representsNonZero(allow_units);}
bool RootFunction::representsEven(const MathStructure&, bool) const {return false;}
bool RootFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool RootFunction::representsUndefined(const MathStructure&) const {return false;}

SquareFunction::SquareFunction() : MathFunction("sq", 1) {
	Argument *arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int SquareFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0];
	mstruct ^= 2;
	return 1;
}

ExpFunction::ExpFunction() : MathFunction("exp", 1) {
	Argument *arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int ExpFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = CALCULATOR->v_e;
	mstruct ^= vargs[0];
	return 1;
}

LogFunction::LogFunction() : MathFunction("ln", 1) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONZERO, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
bool LogFunction::representsPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsPositive() && ((vargs[0].isNumber() && vargs[0].number().isGreaterThan(nr_one)) || (vargs[0].isVariable() && vargs[0].variable()->isKnown() && ((KnownVariable*) vargs[0].variable())->get().isNumber() && ((KnownVariable*) vargs[0].variable())->get().number().isGreaterThan(nr_one)));}
bool LogFunction::representsNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative() && ((vargs[0].isNumber() && vargs[0].number().isLessThan(nr_one)) || (vargs[0].isVariable() && vargs[0].variable()->isKnown() && ((KnownVariable*) vargs[0].variable())->get().isNumber() && ((KnownVariable*) vargs[0].variable())->get().number().isLessThan(nr_one)));}
bool LogFunction::representsNonNegative(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsPositive() && ((vargs[0].isNumber() && vargs[0].number().isGreaterThanOrEqualTo(nr_one)) || (vargs[0].isVariable() && vargs[0].variable()->isKnown() && ((KnownVariable*) vargs[0].variable())->get().isNumber() && ((KnownVariable*) vargs[0].variable())->get().number().isGreaterThanOrEqualTo(nr_one)));}
bool LogFunction::representsNonPositive(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative() && ((vargs[0].isNumber() && vargs[0].number().isLessThanOrEqualTo(nr_one)) || (vargs[0].isVariable() && vargs[0].variable()->isKnown() && ((KnownVariable*) vargs[0].variable())->get().isNumber() && ((KnownVariable*) vargs[0].variable())->get().number().isLessThanOrEqualTo(nr_one)));}
bool LogFunction::representsInteger(const MathStructure&, bool) const {return false;}
bool LogFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units) && vargs[0].representsNonZero(allow_units);}
bool LogFunction::representsRational(const MathStructure&, bool) const {return false;}
bool LogFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsPositive();}
bool LogFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative();}
bool LogFunction::representsComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNegative();}
bool LogFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 1 && (vargs[0].representsNonPositive() || (vargs[0].isNumber() && COMPARISON_IS_NOT_EQUAL(vargs[0].number().compare(nr_one))) || (vargs[0].isVariable() && vargs[0].variable()->isKnown() && ((KnownVariable*) vargs[0].variable())->get().isNumber() && COMPARISON_IS_NOT_EQUAL(((KnownVariable*) vargs[0].variable())->get().number().compare(nr_one))));}
bool LogFunction::representsEven(const MathStructure&, bool) const {return false;}
bool LogFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool LogFunction::representsUndefined(const MathStructure&) const {return false;}
int LogFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;

	mstruct = vargs[0];

	if(mstruct.isVariable() && mstruct.variable() == CALCULATOR->v_e) {
		mstruct.set(m_one);
		return true;
	} else if(mstruct.isPower()) {
		if(mstruct[0].isVariable() && mstruct[0].variable() == CALCULATOR->v_e) {
			if(mstruct[1].representsReal()) {
				mstruct.setToChild(2, true);
				return true;
			}
		} else if(eo.approximation != APPROXIMATION_APPROXIMATE && ((mstruct[0].representsPositive(true) && mstruct[1].representsReal()) || (mstruct[1].isNumber() && mstruct[1].number().isFraction()))) {
			MathStructure mstruct2;
			mstruct2.set(CALCULATOR->f_ln, &mstruct[0], NULL);
			mstruct2 *= mstruct[1];
			mstruct = mstruct2;
			return true;
		}
	}

	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_EXACT;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
		if(mstruct.isVector()) {CALCULATOR->endTemporaryStopMessages(true); return -1;}
	} else {
		mstruct.eval(eo);
		if(mstruct.isVector()) return -1;
	}

	bool b = false;
	if(mstruct.isVariable() && mstruct.variable() == CALCULATOR->v_e) {
		mstruct.set(m_one);
		b = true;
	} else if(mstruct.isPower()) {
		if(mstruct[0].isVariable() && mstruct[0].variable() == CALCULATOR->v_e) {
			if(mstruct[1].representsReal()) {
				mstruct.setToChild(2, true);
				b = true;
			}
		} else if((mstruct[0].representsPositive(true) && mstruct[1].representsReal()) || (mstruct[1].isNumber() && mstruct[1].number().isFraction())) {
			MathStructure mstruct2;
			mstruct2.set(CALCULATOR->f_ln, &mstruct[0], NULL);
			mstruct2 *= mstruct[1];
			mstruct = mstruct2;
			b = true;
		}
	} else if(mstruct.isMultiplication()) {
		b = true;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!mstruct[i].representsPositive()) {
				b = false;
				break;
			}
		}
		if(b) {
			MathStructure mstruct2;
			mstruct2.set(CALCULATOR->f_ln, &mstruct[0], NULL);
			for(size_t i = 1; i < mstruct.size(); i++) {
				mstruct2.add(MathStructure(CALCULATOR->f_ln, &mstruct[i], NULL), i > 1);
			}
			mstruct = mstruct2;
		}
	}
	if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(b);
	if(b) return 1;
	if(eo.approximation == APPROXIMATION_TRY_EXACT && !mstruct.isNumber()) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		mstruct = vargs[0];
		mstruct.eval(eo2);
	}

	if(mstruct.isNumber()) {
		if(eo.allow_complex && mstruct.number().isMinusOne()) {
			mstruct = CALCULATOR->v_i->get();
			mstruct *= CALCULATOR->v_pi;
			return 1;
		} else if(mstruct.number().isI()) {
			mstruct.set(1, 2, 0);
			mstruct *= CALCULATOR->v_pi;
			mstruct *= CALCULATOR->v_i->get();
			return 1;
		} else if(mstruct.number().isMinusI()) {
			mstruct.set(-1, 2, 0);
			mstruct *= CALCULATOR->v_pi;
			mstruct *= CALCULATOR->v_i->get();
			return 1;
		} else if(eo.allow_complex && eo.allow_infinite && mstruct.number().isMinusInfinity()) {
			mstruct = CALCULATOR->v_pi;
			mstruct *= CALCULATOR->v_i->get();
			Number nr; nr.setPlusInfinity();
			mstruct += nr;
			return 1;
		}
		Number nr(mstruct.number());
		if(nr.ln() && !(eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) && !(!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) && !(!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			mstruct.set(nr, true);
			return 1;
		}
		if(mstruct.number().isRational() && mstruct.number().isPositive()) {
			if(mstruct.number().isInteger()) {
				if(mstruct.number().isLessThanOrEqualTo(PRIMES[NR_OF_PRIMES - 1])) {
					vector<Number> factors;
					mstruct.number().factorize(factors);
					if(factors.size() > 1) {
						mstruct.clear(true);
						mstruct.setType(STRUCT_ADDITION);
						for(size_t i = 0; i < factors.size(); i++) {
							if(i > 0 && factors[i] == factors[i - 1]) {
								if(mstruct.last().isMultiplication()) mstruct.last().last().number()++;
								else mstruct.last() *= nr_two;
							} else {
								mstruct.addChild(MathStructure(CALCULATOR->f_ln, NULL));
								mstruct.last().addChild(factors[i]);
							}
						}
						if(mstruct.size() == 1) mstruct.setToChild(1, true);
						return 1;
					}
				}
			} else {
				MathStructure mstruct2(CALCULATOR->f_ln, NULL);
				mstruct2.addChild(mstruct.number().denominator());
				mstruct.number().set(mstruct.number().numerator());
				mstruct.transform(CALCULATOR->f_ln);
				mstruct -= mstruct2;
				return 1;
			}
		} else if(mstruct.number().hasImaginaryPart()) {
			if(mstruct.number().hasRealPart()) {
				MathStructure *marg = new MathStructure(mstruct);
				if(calculate_arg(*marg, eo)) {
					mstruct.transform(CALCULATOR->f_abs);
					mstruct.transform(CALCULATOR->f_ln);
					marg->multiply(CALCULATOR->v_i->get());
					mstruct.add_nocopy(marg);
					return 1;
				}
				marg->unref();
			} else {
				bool b_neg = mstruct.number().imaginaryPartIsNegative();
				if(mstruct.number().abs()) {
					mstruct.transform(this);
					mstruct += b_neg ? nr_minus_half : nr_half;
					mstruct.last() *= CALCULATOR->v_pi;
					mstruct.last().multiply(CALCULATOR->v_i->get(), true);
				}
				return 1;
			}
		}
	} else if(mstruct.isPower()) {
		if((mstruct[0].representsPositive(true) && mstruct[1].representsReal()) || (mstruct[1].isNumber() && mstruct[1].number().isFraction())) {
			MathStructure mstruct2;
			mstruct2.set(CALCULATOR->f_ln, &mstruct[0], NULL);
			mstruct2 *= mstruct[1];
			mstruct = mstruct2;
			return 1;
		}
		if(mstruct[1].isNumber() && mstruct[1].number().isTwo() && mstruct[0].representsPositive()) {
			mstruct.setToChild(1, true);
			mstruct.transform(this);
			mstruct *= nr_two;
			return 1;
		}
		if(eo.approximation == APPROXIMATION_EXACT && !mstruct[1].isNumber()) {
			CALCULATOR->beginTemporaryStopMessages();
			MathStructure mtest(mstruct[1]);
			EvaluationOptions eo2 = eo;
			eo2.approximation = APPROXIMATION_APPROXIMATE;
			mtest.eval(eo2);
			if(!CALCULATOR->endTemporaryStopMessages() && mtest.isNumber() && mtest.number().isFraction()) {
				MathStructure mstruct2;
				mstruct2.set(CALCULATOR->f_ln, &mstruct[0], NULL);
				mstruct2 *= mstruct[1];
				mstruct = mstruct2;
				return 1;
			}
		}
	}
	if(eo.allow_complex && mstruct.representsNegative()) {
		mstruct.negate();
		mstruct.transform(CALCULATOR->f_ln);
		mstruct += CALCULATOR->v_pi;
		mstruct.last() *= nr_one_i;
		return 1;
	}
	if(!mstruct.representsReal()) {
		MathStructure *marg = new MathStructure(mstruct);
		if(calculate_arg(*marg, eo)) {
			mstruct.transform(CALCULATOR->f_abs);
			mstruct.transform(CALCULATOR->f_ln);
			marg->multiply(CALCULATOR->v_i->get());
			mstruct.add_nocopy(marg);
			return 1;
		}
		marg->unref();
	}

	return -1;

}
LognFunction::LognFunction() : MathFunction("log", 1, 2) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONZERO, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
	arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONZERO, false);
	arg->setHandleVector(true);
	setArgumentDefinition(2, arg);
	setDefaultValue(2, "e");
}
int LognFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector() || vargs[1].isVector()) return 0;
	if(vargs[1].isVariable() && vargs[1].variable() == CALCULATOR->v_e) {
		mstruct.set(CALCULATOR->f_ln, &vargs[0], NULL);
		return 1;
	}
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	MathStructure mstructv2 = vargs[1];
	mstructv2.eval(eo);
	if(mstructv2.isVector()) return -2;
	if(mstruct.isPower()) {
		if((mstruct[0].representsPositive(true) && mstruct[1].representsReal()) || (mstruct[1].isNumber() && mstruct[1].number().isFraction())) {
			MathStructure mstruct2;
			mstruct2.set(CALCULATOR->f_logn, &mstruct[0], &mstructv2, NULL);
			mstruct2 *= mstruct[1];
			mstruct = mstruct2;
			return 1;
		}
	} else if(mstruct.isMultiplication()) {
		bool b = true;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!mstruct[i].representsPositive()) {
				b = false;
				break;
			}
		}
		if(b) {
			MathStructure mstruct2;
			mstruct2.set(CALCULATOR->f_logn, &mstruct[0], &mstructv2, NULL);
			for(size_t i = 1; i < mstruct.size(); i++) {
				mstruct2.add(MathStructure(CALCULATOR->f_logn, &mstruct[i], &mstructv2, NULL), i > 1);
			}
			mstruct = mstruct2;
			return 1;
		}
	} else if(mstruct.isNumber() && mstructv2.isNumber()) {
		Number nr(mstruct.number());
		if(nr.log(mstructv2.number()) && !(eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) && !(!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) && !(!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			mstruct.set(nr, true);
			return 1;
		}
	}
	mstruct.set(CALCULATOR->f_ln, &vargs[0], NULL);
	mstruct.divide_nocopy(new MathStructure(CALCULATOR->f_ln, &vargs[1], NULL));
	return 1;
}

bool LambertWFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 2 && (vargs[0].representsNumber() && (vargs[1].isZero() || vargs[0].representsNonZero()));}
bool LambertWFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 2 && (vargs[1].isZero() && vargs[0].representsNonNegative());}
bool LambertWFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 2 && (vargs[0].isZero() || (vargs[1].isZero() && vargs[0].representsNonNegative()));}
bool LambertWFunction::representsComplex(const MathStructure &vargs, bool) const {return vargs.size() == 2 && (vargs[0].representsComplex() || (vargs[0].representsNonZero() && (vargs[1].isInteger() && (!vargs[1].isMinusOne() || vargs[0].representsPositive()) && !vargs[1].isZero())));}
bool LambertWFunction::representsNonZero(const MathStructure &vargs, bool) const {return vargs.size() == 2 && (vargs[1].representsNonZero() || vargs[0].representsNonZero());}

LambertWFunction::LambertWFunction() : MathFunction("lambertw", 1, 2) {
	NumberArgument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setComplexAllowed(false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, false));
	setDefaultValue(2, "0");
}
int LambertWFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];

	if(eo.approximation == APPROXIMATION_TRY_EXACT && vargs[1].isZero()) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_EXACT;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
		if(mstruct.isVector()) {CALCULATOR->endTemporaryStopMessages(true); return -1;}
	} else {
		mstruct.eval(eo);
		if(mstruct.isVector()) return -1;
	}

	if(!vargs[1].isZero()) {
		if(vargs[0].isZero()) {
			mstruct.set(nr_minus_inf, true);
			return 1;
		} else if(vargs[1].isMinusOne()) {
			if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0].isNumber() && vargs[0][1].isPower() && vargs[0][1][0].isVariable() && vargs[0][1][0].variable() == CALCULATOR->v_e && vargs[0][0].number() <= nr_minus_one && vargs[0][1][1] == vargs[0][0]) {
				mstruct = vargs[0][0];
				return 1;
			}
		}
		return 0;
	}

	bool b = false;
	if(mstruct.isZero()) {
		b = true;
	} else if(mstruct.isVariable() && mstruct.variable() == CALCULATOR->v_e) {
		mstruct.set(1, 1, 0, true);
		b = true;
	} else if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[0].isMinusOne() && mstruct[1].isPower() && mstruct[1][0].isVariable() && mstruct[1][0].variable() == CALCULATOR->v_e && mstruct[1][1].isMinusOne()) {
		mstruct.set(-1, 1, 0, true);
		b = true;
	}
	if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(b);
	if(b) return 1;
	if(eo.approximation == APPROXIMATION_EXACT) return -1;
	if(eo.approximation == APPROXIMATION_TRY_EXACT && !mstruct.isNumber()) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		mstruct = vargs[0];
		mstruct.eval(eo2);
	}
	if(mstruct.isNumber()) {
		Number nr(mstruct.number());
		if(!nr.lambertW()) {
			//if(!CALCULATOR->aborted()) CALCULATOR->error(false, _("Argument for %s() must be a real number greater than or equal to -1/e."), preferredDisplayName().name.c_str(), NULL);
		} else if(!(eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) && !(!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) && !(!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			mstruct.set(nr, true);
			return 1;
		}
	}
	return -1;
}

bool is_real_angle_value(const MathStructure &mstruct) {
	if(mstruct.isUnit()) {
		return mstruct.unit() == CALCULATOR->getRadUnit() || mstruct.unit() == CALCULATOR->getDegUnit() || mstruct.unit() == CALCULATOR->getGraUnit();
	} else if(mstruct.isMultiplication()) {
		bool b = false;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!b && mstruct[i].isUnit()) {
				if(mstruct[i].unit() == CALCULATOR->getRadUnit() || mstruct[i].unit() == CALCULATOR->getDegUnit() || mstruct[i].unit() == CALCULATOR->getGraUnit()) {
					b = true;
				} else {
					return false;
				}
			} else if(!mstruct[i].representsReal()) {
				return false;
			}
		}
		return b;
	} else if(mstruct.isAddition()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!is_real_angle_value(mstruct[i])) return false;
		}
		return true;
	}
	return false;
}
bool is_infinite_angle_value(const MathStructure &mstruct) {
	if(mstruct.isMultiplication() && mstruct.size() == 2) {
		bool b = false;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!b && mstruct[i].isUnit()) {
				if(mstruct[i].unit() == CALCULATOR->getRadUnit() || mstruct[i].unit() == CALCULATOR->getDegUnit() || mstruct[i].unit() == CALCULATOR->getGraUnit()) {
					b = true;
				} else {
					return false;
				}
			} else if(!mstruct[i].isNumber() || !mstruct[i].number().isInfinite()) {
				return false;
			}
		}
		return b;
	}
	return false;
}
bool is_number_angle_value(const MathStructure &mstruct, bool allow_infinity = false) {
	if(mstruct.isUnit()) {
		return mstruct.unit() == CALCULATOR->getRadUnit() || mstruct.unit() == CALCULATOR->getDegUnit() || mstruct.unit() == CALCULATOR->getGraUnit();
	} else if(mstruct.isMultiplication()) {
		bool b = false;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!b && mstruct[i].isUnit()) {
				if(mstruct[i].unit() == CALCULATOR->getRadUnit() || mstruct[i].unit() == CALCULATOR->getDegUnit() || mstruct[i].unit() == CALCULATOR->getGraUnit()) {
					b = true;
				} else {
					return false;
				}
			} else if(!mstruct[i].representsNumber()) {
				if(!allow_infinity || ((!mstruct[i].isNumber() || !mstruct[i].number().isInfinite()) && (!mstruct[i].isPower() || !mstruct[i][0].representsNumber() || !mstruct[i][1].representsNumber())) || mstruct[i].representsUndefined(true)) {
					return false;
				}
			}
		}
		return b;
	} else if(mstruct.isAddition()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!is_number_angle_value(mstruct[i])) return false;
		}
		return true;
	}
	return false;
}

bool has_predominately_negative_sign(const MathStructure &mstruct) {
	if(mstruct.hasNegativeSign() && !mstruct.containsType(STRUCT_ADDITION, true)) return true;
	if(mstruct.isAddition() && mstruct.size() > 0) {
		size_t p_count = 0;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].hasNegativeSign()) {
				p_count++;
				if(p_count > mstruct.size() / 2) return true;
			}
		}
		if(mstruct.size() % 2 == 0 && p_count == mstruct.size() / 2) return mstruct[0].hasNegativeSign();
	}
	return false;
}

void negate_struct(MathStructure &mstruct) {
	if(mstruct.isAddition()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			mstruct[i].negate();
		}
	} else {
		mstruct.negate();
	}
}

bool trig_remove_i(MathStructure &mstruct) {
	if(mstruct.isNumber() && mstruct.number().hasImaginaryPart() && !mstruct.number().hasRealPart()) {
		mstruct.number() /= nr_one_i;
		return true;
	} else if(mstruct.isMultiplication() && mstruct.size() > 1 && mstruct[0].isNumber() && mstruct[0].number().hasImaginaryPart() && !mstruct[0].number().hasRealPart()) {
		mstruct[0].number() /= nr_one_i;
		return true;
	} else if(mstruct.isAddition() && mstruct.size() > 0) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!(mstruct[i].isNumber() && mstruct[i].number().hasImaginaryPart() && !mstruct[i].number().hasRealPart()) && !(mstruct[i].isMultiplication() && mstruct[i].size() > 1 && mstruct[i][0].isNumber() && mstruct[i][0].number().hasImaginaryPart() && !mstruct[i][0].number().hasRealPart())) {
				return false;
			}
		}
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].isNumber()) mstruct[i].number() /= nr_one_i;
			else mstruct[i][0].number() /= nr_one_i;
		}
		return true;
	}
	return false;
}

SinFunction::SinFunction() : MathFunction("sin", 1) {
	Argument *arg = new AngleArgument();
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
bool SinFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && ((allow_units && (vargs[0].representsNumber(true) || vargs[0].representsNonComplex(true))) || (!allow_units && is_number_angle_value(vargs[0], true)));}
bool SinFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && (is_real_angle_value(vargs[0]) || is_infinite_angle_value(vargs[0]));}
bool SinFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex(true);}
int SinFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	if(vargs[0].isVector()) return 0;
	if(CALCULATOR->getRadUnit()) {
		if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getRadUnit()) {
			mstruct = vargs[0][0];
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getRadUnit()) {
			mstruct = vargs[0][1];
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getDegUnit()) {
			mstruct = vargs[0][0];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 180), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getDegUnit()) {
			mstruct = vargs[0][1];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 180), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getGraUnit()) {
			mstruct = vargs[0][0];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 200), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getGraUnit()) {
			mstruct = vargs[0][1];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 200), true);
		} else {
			mstruct = vargs[0];
			mstruct.convert(CALCULATOR->getRadUnit());
			mstruct /= CALCULATOR->getRadUnit();
		}
	} else {
		mstruct = vargs[0];
	}

	MathFunction *f = NULL;
	if(eo.approximation == APPROXIMATION_APPROXIMATE && (eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES || eo.parse_options.angle_unit == ANGLE_UNIT_GRADIANS)) {
		if(mstruct.isMultiplication() && mstruct.size() == 3 && mstruct[0].isFunction() && mstruct[0].size() == 1 && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi && mstruct[2].isNumber() && mstruct[2].number().equals(Number(1, eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES ? 180 : 200))) {
			f = mstruct[0].function();
		}
	}

	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_EXACT;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
	} else if(!f) {
		mstruct.eval(eo);
	}

	if(mstruct.isVector()) {
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		if(CALCULATOR->getRadUnit()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				mstruct[i] *= CALCULATOR->getRadUnit();
			}
		}
		return -1;
	}

	bool b = false, b_recalc = true;

	if(eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES || eo.parse_options.angle_unit == ANGLE_UNIT_GRADIANS) {
		if(!f && mstruct.isMultiplication() && mstruct.size() == 3 && mstruct[2].isFunction() && mstruct[2].size() == 1 && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi && mstruct[0].isNumber() && mstruct[0].number().equals(Number(1, eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES ? 180 : 200))) {
			f = mstruct[2].function();
		}
	} else if(mstruct.isFunction() && mstruct.size() == 1) {
		f = mstruct.function();
	}

	if(mstruct.isVariable() && mstruct.variable() == CALCULATOR->v_pi) {
		mstruct.clear();
		b = true;
	} else if(f) {
		if(f == CALCULATOR->f_asin) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1, true);
			b = true;
		} else if(f == CALCULATOR->f_acos) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1);
			mstruct ^= nr_two;
			mstruct.negate();
			mstruct += nr_one;
			mstruct ^= nr_half;
			b = true;
		} else if(f == CALCULATOR->f_atan && (mstruct.isFunction() ? !mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true) : (mstruct[0][0].isFunction() ? mstruct[2][0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true) : mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true)))) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1);
			MathStructure *mmul = new MathStructure(mstruct);
			mstruct ^= nr_two;
			mstruct += nr_one;
			mstruct ^= nr_minus_half;
			mstruct.multiply_nocopy(mmul);
			b = true;
		}
	} else if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[0].isNumber() && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi) {
		if(mstruct[0].number().isInteger()) {
			mstruct.clear();
			b = true;
		} else if(!mstruct[0].number().hasImaginaryPart() && !mstruct[0].number().includesInfinity() && !mstruct[0].number().isInterval()) {
			Number nr(mstruct[0].number());
			nr.frac();
			Number nr_int(mstruct[0].number());
			nr_int.floor();
			bool b_even = nr_int.isEven();
			nr.setNegative(false);
			if(nr.equals(Number(1, 2, 0))) {
				if(b_even) mstruct.set(1, 1, 0);
				else mstruct.set(-1, 1, 0);
				b = true;
			} else if(nr.equals(Number(1, 4, 0)) || nr.equals(Number(3, 4, 0))) {
				mstruct.set(2, 1, 0);
				mstruct.raise_nocopy(new MathStructure(1, 2, 0));
				mstruct.divide_nocopy(new MathStructure(2, 1, 0));
				if(!b_even) mstruct.negate();
				b = true;
			} else if(nr.equals(Number(1, 3, 0)) || nr.equals(Number(2, 3, 0))) {
				mstruct.set(3, 1, 0);
				mstruct.raise_nocopy(new MathStructure(1, 2, 0));
				mstruct.divide_nocopy(new MathStructure(2, 1, 0));
				if(!b_even) mstruct.negate();
				b = true;
			} else if(nr.equals(Number(1, 6, 0)) || nr.equals(Number(5, 6, 0))) {
				if(b_even) mstruct.set(1, 2, 0);
				else mstruct.set(-1, 2, 0);
				b = true;
			} else if(eo.approximation == APPROXIMATION_EXACT && (mstruct[0].number().isNegative() || !mstruct[0].number().isFraction() || mstruct[0].number().isGreaterThan(nr_half))) {
				nr_int = mstruct[0].number();
				nr_int.floor();
				Number nr_frac = mstruct[0].number();
				nr_frac -= nr_int;
				if(nr_frac.isGreaterThan(nr_half)) {
					nr_frac -= nr_half;
					mstruct[0].number() = nr_half;
					mstruct[0].number() -= nr_frac;
				} else {
					mstruct[0].number() = nr_frac;
				}
				if(nr_int.isOdd()) {
					if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
					mstruct.transform(this);
					mstruct.negate();
					b = true;
				}
			}
		}
	} else if(mstruct.isAddition()) {
		size_t i = 0;
		bool b_negate = false;
		for(; i < mstruct.size(); i++) {
			if((mstruct[i].isVariable() && mstruct[i].variable() == CALCULATOR->v_pi) || (mstruct[i].isMultiplication() && mstruct[i].size() == 2 && mstruct[i][1] == CALCULATOR->v_pi && mstruct[i][0].isNumber())) {
				if(mstruct[i].isVariable() || mstruct[i][0].number().isInteger()) {
					b_negate = mstruct[i].isVariable() || mstruct[i][0].number().isOdd();
					mstruct.delChild(i + 1);
					b_recalc = false;
					break;
				} else if(mstruct[i][0].number().isReal() && (mstruct[i][0].number().isNegative() || !mstruct[i][0].number().isFraction())) {
					Number nr_int = mstruct[i][0].number();
					nr_int.floor();
					mstruct[i][0].number() -= nr_int;
					b_negate = nr_int.isOdd();
					b_recalc = false;
					break;
				}
			}
		}
		b = b_negate;
		if(b_negate) {
			if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
			mstruct.transform(this);
			mstruct.negate();
		}
	}
	if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(b);
	if(b) return 1;
	if(eo.approximation == APPROXIMATION_TRY_EXACT && !mstruct.isNumber()) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		if(b_recalc) {
			mstruct = vargs[0];
			if(CALCULATOR->getRadUnit()) {
				mstruct.convert(CALCULATOR->getRadUnit());
				mstruct /= CALCULATOR->getRadUnit();
			}
		}
		mstruct.eval(eo2);
	}

	if(mstruct.isNumber()) {
		Number nr(mstruct.number());
		if(nr.sin() && !(eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) && !(!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) && !(!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			mstruct.set(nr, true);
			return 1;
		}
	}

	if(trig_remove_i(mstruct)) {
		mstruct.transform(CALCULATOR->f_sinh);
		mstruct *= nr_one_i;
		return 1;
	}

	if(has_predominately_negative_sign(mstruct)) {
		negate_struct(mstruct);
		if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
		mstruct.transform(this);
		mstruct.negate();
		return 1;
	}


	if(CALCULATOR->getRadUnit()) {
		if(mstruct.isVector()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				mstruct[i] *= CALCULATOR->getRadUnit();
			}
		} else {
			mstruct *= CALCULATOR->getRadUnit();
		}
	}
	return -1;
}

CosFunction::CosFunction() : MathFunction("cos", 1) {
	Argument *arg = new AngleArgument();
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
bool CosFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && ((allow_units && (vargs[0].representsNumber(true) || vargs[0].representsNonComplex(true))) || (!allow_units && is_number_angle_value(vargs[0], true)));}
bool CosFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && (is_real_angle_value(vargs[0]) || is_infinite_angle_value(vargs[0]));}
bool CosFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex(true);}
int CosFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	if(vargs[0].isVector()) return 0;
	if(CALCULATOR->getRadUnit()) {
		if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getRadUnit()) {
			mstruct = vargs[0][0];
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getRadUnit()) {
			mstruct = vargs[0][1];
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getDegUnit()) {
			mstruct = vargs[0][0];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 180), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getDegUnit()) {
			mstruct = vargs[0][1];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 180), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getGraUnit()) {
			mstruct = vargs[0][0];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 200), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getGraUnit()) {
			mstruct = vargs[0][1];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 200), true);
		} else {
			mstruct = vargs[0];
			mstruct.convert(CALCULATOR->getRadUnit());
			mstruct /= CALCULATOR->getRadUnit();
		}
	} else {
		mstruct = vargs[0];
	}

	MathFunction *f = NULL;
	if(eo.approximation == APPROXIMATION_APPROXIMATE && (eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES || eo.parse_options.angle_unit == ANGLE_UNIT_GRADIANS)) {
		if(mstruct.isMultiplication() && mstruct.size() == 3 && mstruct[0].isFunction() && mstruct[0].size() == 1 && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi && mstruct[2].isNumber() && mstruct[2].number().equals(Number(1, eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES ? 180 : 200))) {
			f = mstruct[0].function();
		}
	}

	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_EXACT;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
	} else if(!f) {
		mstruct.eval(eo);
	}

	if(mstruct.isVector()) {
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		if(CALCULATOR->getRadUnit()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				mstruct[i] *= CALCULATOR->getRadUnit();
			}
		}
		return -1;
	}

	bool b = false, b_recalc = true;

	if(eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES || eo.parse_options.angle_unit == ANGLE_UNIT_GRADIANS) {
		if(!f && mstruct.isMultiplication() && mstruct.size() == 3 && mstruct[2].isFunction() && mstruct[2].size() == 1 && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi && mstruct[0].isNumber() && mstruct[0].number().equals(Number(1, eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES ? 180 : 200))) {
			f = mstruct[2].function();
		}
	} else if(mstruct.isFunction() && mstruct.size() == 1) {
		f = mstruct.function();
	}

	if(mstruct.isVariable() && mstruct.variable() == CALCULATOR->v_pi) {
		mstruct = -1;
		b = true;
	} else if(f) {
		if(f == CALCULATOR->f_acos) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1, true);
			b = true;
		} else if(f == CALCULATOR->f_asin) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1);
			mstruct ^= nr_two;
			mstruct.negate();
			mstruct += nr_one;
			mstruct ^= nr_half;
			b = true;
		} else if(f == CALCULATOR->f_atan) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1);
			mstruct ^= nr_two;
			mstruct += nr_one;
			mstruct ^= nr_minus_half;
			b = true;
		}
	} else if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[0].isNumber() && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi) {
		if(mstruct[0].number().isInteger()) {
			if(mstruct[0].number().isEven()) {
				mstruct = 1;
			} else {
				mstruct = -1;
			}
			b = true;
		} else if(!mstruct[0].number().hasImaginaryPart() && !mstruct[0].number().includesInfinity() && !mstruct[0].number().isInterval()) {
			Number nr(mstruct[0].number());
			nr.frac();
			Number nr_int(mstruct[0].number());
			nr_int.trunc();
			bool b_even = nr_int.isEven();
			nr.setNegative(false);
			if(nr.equals(Number(1, 2, 0))) {
				mstruct.clear();
				b = true;
			} else if(nr.equals(Number(1, 4, 0))) {
				mstruct.set(2, 1, 0);
				mstruct.raise_nocopy(new MathStructure(1, 2, 0));
				mstruct.divide_nocopy(new MathStructure(2, 1, 0));
				if(!b_even) mstruct.negate();
				b = true;
			} else if(nr.equals(Number(3, 4, 0))) {
				mstruct.set(2, 1, 0);
				mstruct.raise_nocopy(new MathStructure(1, 2, 0));
				mstruct.divide_nocopy(new MathStructure(2, 1, 0));
				if(b_even) mstruct.negate();
				b = true;
			} else if(nr.equals(Number(1, 3, 0))) {
				if(b_even) mstruct.set(1, 2, 0);
				else mstruct.set(-1, 2, 0);
				b = true;
			} else if(nr.equals(Number(2, 3, 0))) {
				if(b_even) mstruct.set(-1, 2, 0);
				else mstruct.set(1, 2, 0);
				b = true;
			} else if(nr.equals(Number(1, 6, 0))) {
				mstruct.set(3, 1, 0);
				mstruct.raise_nocopy(new MathStructure(1, 2, 0));
				mstruct.divide_nocopy(new MathStructure(2, 1, 0));
				if(!b_even) mstruct.negate();
				b = true;
			} else if(nr.equals(Number(5, 6, 0))) {
				mstruct.set(3, 1, 0);
				mstruct.raise_nocopy(new MathStructure(1, 2, 0));
				mstruct.divide_nocopy(new MathStructure(2, 1, 0));
				if(b_even) mstruct.negate();
				b = true;
			} else if(eo.approximation == APPROXIMATION_EXACT && (mstruct[0].number().isNegative() || !mstruct[0].number().isFraction() || mstruct[0].number().isGreaterThan(nr_half))) {
				nr_int = mstruct[0].number();
				nr_int.floor();
				Number nr_frac = mstruct[0].number();
				nr_frac -= nr_int;
				if(nr_frac.isGreaterThan(nr_half)) {
					nr_frac -= nr_half;
					mstruct[0].number() = nr_half;
					mstruct[0].number() -= nr_frac;
					nr_int++;
				} else {
					mstruct[0].number() = nr_frac;
				}
				if(nr_int.isOdd()) {
					if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
					mstruct.transform(this);
					mstruct.negate();
					b = true;
				}
			}
		}
	} else if(mstruct.isAddition()) {
		size_t i = 0;
		bool b_negate = false;
		for(; i < mstruct.size(); i++) {
			if((mstruct[i].isVariable() && mstruct[i].variable() == CALCULATOR->v_pi) || (mstruct[i].isMultiplication() && mstruct[i].size() == 2 && mstruct[i][1] == CALCULATOR->v_pi && mstruct[i][0].isNumber())) {
				if(mstruct[i].isVariable() || mstruct[i][0].number().isInteger()) {
					b_negate = mstruct[i].isVariable() || mstruct[i][0].number().isOdd();
					mstruct.delChild(i + 1);
					b_recalc = false;
					break;
				} else if(mstruct[i][0].number().isReal() && (mstruct[i][0].number().isNegative() || !mstruct[i][0].number().isFraction())) {
					Number nr_int = mstruct[i][0].number();
					nr_int.floor();
					mstruct[i][0].number() -= nr_int;
					b_negate = nr_int.isOdd();
					b_recalc = false;
					break;
				}
			}
		}
		b = b_negate;
		if(b_negate) {
			if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
			mstruct.transform(this);
			mstruct.negate();
		}
	}
	if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(b);
	if(b) return 1;
	if(eo.approximation == APPROXIMATION_TRY_EXACT && !mstruct.isNumber()) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		if(b_recalc) {
			mstruct = vargs[0];
			if(CALCULATOR->getRadUnit()) {
				mstruct.convert(CALCULATOR->getRadUnit());
				mstruct /= CALCULATOR->getRadUnit();
			}
		}
		mstruct.eval(eo2);
	}
	if(mstruct.isNumber()) {
		Number nr(mstruct.number());
		if(nr.cos() && !(eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) && !(!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) && !(!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			mstruct.set(nr, true);
			return 1;
		}
	}
	if(trig_remove_i(mstruct)) {
		mstruct.transform(CALCULATOR->f_cosh);
		return 1;
	}
	if(has_predominately_negative_sign(mstruct)) {
		negate_struct(mstruct);
	}
	if(CALCULATOR->getRadUnit()) {
		if(mstruct.isVector()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				mstruct[i] *= CALCULATOR->getRadUnit();
			}
		} else {
			mstruct *= CALCULATOR->getRadUnit();
		}
	}
	return -1;
}

TanFunction::TanFunction() : MathFunction("tan", 1) {
	Argument *arg = new AngleArgument();
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
bool TanFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && ((allow_units && vargs[0].representsNumber(true)) || (!allow_units && is_number_angle_value(vargs[0], true)));}
bool TanFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && is_real_angle_value(vargs[0]);}
bool TanFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex(true);}
int TanFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	if(vargs[0].isVector()) return 0;
	if(CALCULATOR->getRadUnit()) {
		if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getRadUnit()) {
			mstruct = vargs[0][0];
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getRadUnit()) {
			mstruct = vargs[0][1];
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getDegUnit()) {
			mstruct = vargs[0][0];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 180), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getDegUnit()) {
			mstruct = vargs[0][1];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 180), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getGraUnit()) {
			mstruct = vargs[0][0];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 200), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getGraUnit()) {
			mstruct = vargs[0][1];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 200), true);
		} else {
			mstruct = vargs[0];
			mstruct.convert(CALCULATOR->getRadUnit());
			mstruct /= CALCULATOR->getRadUnit();
		}
	} else {
		mstruct = vargs[0];
	}

	MathFunction *f = NULL;
	if(eo.approximation == APPROXIMATION_APPROXIMATE && (eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES || eo.parse_options.angle_unit == ANGLE_UNIT_GRADIANS)) {
		if(mstruct.isMultiplication() && mstruct.size() == 3 && mstruct[0].isFunction() && mstruct[0].size() == 1 && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi && mstruct[2].isNumber() && mstruct[2].number().equals(Number(1, eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES ? 180 : 200))) {
			f = mstruct[0].function();
		}
	}

	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_EXACT;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
	} else if(!f) {
		mstruct.eval(eo);
	}

	if(mstruct.isVector()) {
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		if(CALCULATOR->getRadUnit()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				mstruct[i] *= CALCULATOR->getRadUnit();
			}
		}
		return -1;
	}

	bool b = false, b_recalc = true;

	if(eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES || eo.parse_options.angle_unit == ANGLE_UNIT_GRADIANS) {
		if(!f && mstruct.isMultiplication() && mstruct.size() == 3 && mstruct[2].isFunction() && mstruct[2].size() == 1 && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi && mstruct[0].isNumber() && mstruct[0].number().equals(Number(1, eo.parse_options.angle_unit == ANGLE_UNIT_DEGREES ? 180 : 200))) {
			f = mstruct[2].function();
		}
	} else if(mstruct.isFunction() && mstruct.size() == 1) {
		f = mstruct.function();
	}

	if(mstruct.isVariable() && mstruct.variable() == CALCULATOR->v_pi) {
		mstruct.clear();
		b = true;
	} else if(f) {
		if(f == CALCULATOR->f_atan) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1, true);
			b = true;
		} else if(f == CALCULATOR->f_asin && (mstruct.isFunction() ? !mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true) : (mstruct[0][0].isFunction() ? mstruct[2][0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true) : mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true)))) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1);
			MathStructure *mmul = new MathStructure(mstruct);
			mstruct ^= nr_two;
			mstruct.negate();
			mstruct += nr_one;
			mstruct ^= nr_minus_half;
			mstruct.multiply_nocopy(mmul);
			b = true;
		} else if(f == CALCULATOR->f_acos && (mstruct.isFunction() ? !mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true) : (mstruct[0][0].isFunction() ? mstruct[2][0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true) : mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation != APPROXIMATION_EXACT, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true)))) {
			if(!mstruct.isFunction()) mstruct.setToChild(mstruct[0].isFunction() ? 1 : 3, true);
			mstruct.setToChild(1);
			MathStructure *mmul = new MathStructure(mstruct);
			mstruct ^= nr_two;
			mstruct.negate();
			mstruct += nr_one;
			mstruct ^= nr_half;
			mstruct.divide_nocopy(mmul);
			b = true;
		}
	} else if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[0].isNumber() && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi) {
		if(mstruct[0].number().isInteger()) {
			mstruct.clear();
			b = true;
		} else if(!mstruct[0].number().hasImaginaryPart() && !mstruct[0].number().includesInfinity() && !mstruct[0].number().isInterval()) {
			Number nr(mstruct[0].number());
			nr.frac();
			bool b_neg = nr.isNegative();
			nr.setNegative(false);
			if(nr.equals(nr_half)) {
				Number nr_int(mstruct[0].number());
				nr_int.floor();
				bool b_even = nr_int.isEven();
				if(b_even) nr.setPlusInfinity();
				else nr.setMinusInfinity();
				mstruct.set(nr);
				b = true;
			} else if(nr.equals(Number(1, 4, 0))) {
				if(b_neg) mstruct.set(-1, 1, 0);
				else mstruct.set(1, 1, 0);
				b = true;
			} else if(nr.equals(Number(3, 4, 0))) {
				if(!b_neg) mstruct.set(-1, 1, 0);
				else mstruct.set(1, 1, 0);
				b = true;
			} else if(nr.equals(Number(1, 3, 0))) {
				mstruct.set(3, 1, 0);
				mstruct.raise_nocopy(new MathStructure(1, 2, 0));
				if(b_neg) mstruct.negate();
				b = true;
			} else if(nr.equals(Number(2, 3, 0))) {
				mstruct.set(3, 1, 0);
				mstruct.raise_nocopy(new MathStructure(1, 2, 0));
				if(!b_neg) mstruct.negate();
				b = true;
			} else if(nr.equals(Number(1, 6, 0))) {
				mstruct.set(3, 1, 0);
				mstruct.raise_nocopy(new MathStructure(-1, 2, 0));
				if(b_neg) mstruct.negate();
				b = true;
			} else if(nr.equals(Number(5, 6, 0))) {
				mstruct.set(3, 1, 0);
				mstruct.raise_nocopy(new MathStructure(-1, 2, 0));
				if(!b_neg) mstruct.negate();
				b = true;
			} else if(eo.approximation == APPROXIMATION_EXACT && (mstruct[0].number().isNegative() || !mstruct[0].number().isFraction() || mstruct[0].number().isGreaterThan(nr_half))) {
				Number nr_int(mstruct[0].number());
				nr_int.floor();
				Number nr_frac = mstruct[0].number();
				nr_frac -= nr_int;
				if(nr_frac.isGreaterThan(nr_half)) {
					nr_frac -= nr_half;
					mstruct[0].number() = nr_half;
					mstruct[0].number() -= nr_frac;
					if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
					mstruct.transform(this);
					mstruct.negate();
					b = true;
				} else {
					mstruct[0].number() = nr_frac;
				}
			}
		}
	} else if(mstruct.isAddition()) {
		size_t i = 0;
		for(; i < mstruct.size(); i++) {
			if((mstruct[i].isVariable() && mstruct[i].variable() == CALCULATOR->v_pi) || (mstruct[i].isMultiplication() && mstruct[i].size() == 2 && mstruct[i][1] == CALCULATOR->v_pi && mstruct[i][0].isNumber())) {
				if(mstruct[i].isVariable() || mstruct[i][0].number().isInteger()) {
					mstruct.delChild(i + 1);
					b_recalc = false;
					break;
				} else if(mstruct[i][0].number().isReal() && (mstruct[i][0].number().isNegative() || !mstruct[i][0].number().isFraction())) {
					Number nr_int = mstruct[i][0].number();
					nr_int.floor();
					mstruct[i][0].number() -= nr_int;
					b_recalc = false;
					break;
				}
			}
		}
	}
	if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(b);
	if(b) return 1;
	if(eo.approximation == APPROXIMATION_TRY_EXACT && !mstruct.isNumber()) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		if(b_recalc) {
			mstruct = vargs[0];
			if(CALCULATOR->getRadUnit()) {
				mstruct.convert(CALCULATOR->getRadUnit());
				mstruct /= CALCULATOR->getRadUnit();
			}
			mstruct.eval(eo2);
		}
	}

	if(mstruct.isNumber()) {
		Number nr(mstruct.number());
		if(nr.tan() && !(eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) && !(!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) && !(!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			mstruct.set(nr, true);
			return 1;
		}
	}

	if(trig_remove_i(mstruct)) {
		mstruct.transform(CALCULATOR->f_tanh);
		mstruct *= nr_one_i;
		return 1;
	}

	if(has_predominately_negative_sign(mstruct)) {
		negate_struct(mstruct);
		if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();
		mstruct.transform(this);
		mstruct.negate();
		return 1;
	}

	if(CALCULATOR->getRadUnit()) {
		if(mstruct.isVector()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				mstruct[i] *= CALCULATOR->getRadUnit();
			}
		} else {
			mstruct *= CALCULATOR->getRadUnit();
		}
	}

	return -1;
}

AsinFunction::AsinFunction() : MathFunction("asin", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool AsinFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
int AsinFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_EXACT;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
	} else {
		mstruct.eval(eo);
	}
	if(mstruct.isVector()) {
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		return -1;
	}
	if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[0] == nr_half && mstruct[1].isPower() && mstruct[1][1] == nr_half) {
		if(mstruct[1][0] == nr_two) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.set(45, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.set(50, 1, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct.set(1, 4, 0); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				default: {mstruct.set(1, 4, 0);	mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();}}
			}
			if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
			return 1;
		} else if(mstruct[1][0] == nr_three) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.set(60, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.set(200, 3, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct.set(1, 3, 0); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				default: {mstruct.set(1, 3, 0);	mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();}}
			}
			if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
			return 1;
		}
	} else if(mstruct.isPower() && mstruct[1] == nr_minus_half && mstruct[0] == nr_two) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {mstruct.set(45, 1, 0); break;}
			case ANGLE_UNIT_GRADIANS: {mstruct.set(50, 1, 0); break;}
			case ANGLE_UNIT_RADIANS: {mstruct.set(1, 4, 0); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			default: {mstruct.set(1, 4, 0);	mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();}}
		}
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		return 1;
	}
	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		if(!mstruct.isNumber()) {
			CALCULATOR->endTemporaryStopMessages(false);
			EvaluationOptions eo2 = eo;
			eo2.approximation = APPROXIMATION_APPROXIMATE;
			mstruct = vargs[0];
			mstruct.eval(eo2);
		} else {
			CALCULATOR->endTemporaryStopMessages(true);
		}
	}
	if(!mstruct.isNumber()) {
		if(trig_remove_i(mstruct)) {
			mstruct.transform(CALCULATOR->f_asinh);
			mstruct *= nr_one_i;
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.multiply_nocopy(new MathStructure(180, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.multiply_nocopy(new MathStructure(200, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				case ANGLE_UNIT_RADIANS: {break;}
				default: {if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
			}
			return 1;
		}
		if(has_predominately_negative_sign(mstruct)) {negate_struct(mstruct); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	if(mstruct.number().isZero()) {
		mstruct.clear();
	} else if(mstruct.number().isOne()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(90, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(100, 1, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else if(mstruct.number().isMinusOne()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(-90, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(-100, 1, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(-1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(-1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else if(mstruct.number().equals(nr_half)) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(30, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(100, 3, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(1, 6, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(1, 6, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else {
		Number nr = mstruct.number();
		if(!nr.asin() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) || (!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			if(trig_remove_i(mstruct)) {
				mstruct.transform(CALCULATOR->f_asinh);
				mstruct *= nr_one_i;
				switch(eo.parse_options.angle_unit) {
					case ANGLE_UNIT_DEGREES: {mstruct.multiply_nocopy(new MathStructure(180, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
					case ANGLE_UNIT_GRADIANS: {mstruct.multiply_nocopy(new MathStructure(200, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
					case ANGLE_UNIT_RADIANS: {break;}
					default: {if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
				}
				return 1;
			}
			if(has_predominately_negative_sign(mstruct)) {mstruct.number().negate(); mstruct.transform(this); mstruct.negate(); return 1;}
			return -1;
		}
		mstruct = nr;
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {mstruct.multiply_nocopy(new MathStructure(180, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			case ANGLE_UNIT_GRADIANS: {mstruct.multiply_nocopy(new MathStructure(200, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			case ANGLE_UNIT_RADIANS: {break;}
			default: {if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
		}
	}
	return 1;

}

AcosFunction::AcosFunction() : MathFunction("acos", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool AcosFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
int AcosFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_EXACT;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
	} else {
		mstruct.eval(eo);
	}
	if(mstruct.isVector()) {
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		return -1;
	}
	if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[0] == nr_half && mstruct[1].isPower() && mstruct[1][1] == nr_half) {
		if(mstruct[1][0] == nr_two) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.set(45, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.set(50, 1, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct.set(1, 4, 0); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				default: {mstruct.set(1, 4, 0);	mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();}}
			}
			if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
			return 1;
		} else if(mstruct[1][0] == nr_three) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.set(30, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.set(100, 3, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct.set(1, 6, 0); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				default: {mstruct.set(1, 6, 0);	mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();}}
			}
			if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
			return 1;
		}
	} else if(mstruct.isPower() && mstruct[1] == nr_minus_half && mstruct[0] == nr_two) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {mstruct.set(45, 1, 0); break;}
			case ANGLE_UNIT_GRADIANS: {mstruct.set(50, 1, 0); break;}
			case ANGLE_UNIT_RADIANS: {mstruct.set(1, 4, 0); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			default: {mstruct.set(1, 4, 0);	mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();}}
		}
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		return 1;
	}
	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		if(!mstruct.isNumber()) {
			CALCULATOR->endTemporaryStopMessages(false);
			EvaluationOptions eo2 = eo;
			eo2.approximation = APPROXIMATION_APPROXIMATE;
			mstruct = vargs[0];
			mstruct.eval(eo2);
		} else {
			CALCULATOR->endTemporaryStopMessages(true);
		}
	}
	if(!mstruct.isNumber()) {
		if(has_predominately_negative_sign(mstruct)) {
			negate_struct(mstruct); mstruct.transform(CALCULATOR->f_asin);
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct += Number(90, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct += Number(100, 1, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct += CALCULATOR->v_pi; mstruct.last() *= nr_half; break;}
				default: {mstruct += CALCULATOR->v_pi; mstruct.last() *= nr_half; if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
			}
			return 1;
		}
		return -1;
	}
	if(mstruct.number().isZero()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(90, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(100, 1, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else if(mstruct.number().isOne()) {
		mstruct.clear();
	} else if(mstruct.number().isMinusOne()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(180, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(200, 1, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(CALCULATOR->v_pi);
				break;
			}
			default: {
				mstruct.set(CALCULATOR->v_pi);
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else if(mstruct.number().equals(nr_half)) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(60, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(200, 3, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(1, 3, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(1, 3, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else {
		Number nr = mstruct.number();
		if(!nr.acos() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) || (!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			if(has_predominately_negative_sign(mstruct)) {
				mstruct.number().negate();
				mstruct.transform(CALCULATOR->f_asin);
				switch(eo.parse_options.angle_unit) {
					case ANGLE_UNIT_DEGREES: {mstruct += Number(90, 1, 0); break;}
					case ANGLE_UNIT_GRADIANS: {mstruct += Number(100, 1, 0); break;}
					case ANGLE_UNIT_RADIANS: {mstruct += CALCULATOR->v_pi; mstruct.last() *= nr_half; break;}
					default: {mstruct += CALCULATOR->v_pi; mstruct.last() *= nr_half; if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
				}
				return 1;
			}
			return -1;
		}
		mstruct = nr;
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {mstruct.multiply_nocopy(new MathStructure(180, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			case ANGLE_UNIT_GRADIANS: {mstruct.multiply_nocopy(new MathStructure(200, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			case ANGLE_UNIT_RADIANS: {break;}
			default: {if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
		}
	}
	return 1;

}

AtanFunction::AtanFunction() : MathFunction("atan", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool AtanFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && (vargs[0].representsReal(allow_units) || (vargs[0].isNumber() && !vargs[0].number().isI() && !vargs[0].number().isMinusI()));}
bool AtanFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool AtanFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex();}
int AtanFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_EXACT;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
	} else {
		mstruct.eval(eo);
	}
	if(mstruct.isVector()) {
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		return -1;
	}
	if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[0].isNumber() && mstruct[1].isPower() && mstruct[1][1] == nr_half && mstruct[1][0] == nr_three && mstruct[0].number() == Number(1, 3)) {
		if(mstruct[1][0] == nr_three) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.set(30, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.set(100, 3, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct.set(1, 6, 0); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				default: {mstruct.set(1, 6, 0);	mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();}}
			}
			if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
			return 1;
		}
	} else if(mstruct.isPower() && mstruct[1] == nr_half && mstruct[0] == nr_three) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {mstruct.set(60, 1, 0); break;}
			case ANGLE_UNIT_GRADIANS: {mstruct.set(200, 3, 0); break;}
			case ANGLE_UNIT_RADIANS: {mstruct.set(1, 3, 0); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			default: {mstruct.set(1, 3, 0);	mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();}}
		}
		if(eo.approximation == APPROXIMATION_TRY_EXACT) CALCULATOR->endTemporaryStopMessages(true);
		return 1;
	}
	if(eo.approximation == APPROXIMATION_TRY_EXACT) {
		if(!mstruct.isNumber()) {
			CALCULATOR->endTemporaryStopMessages(false);
			EvaluationOptions eo2 = eo;
			eo2.approximation = APPROXIMATION_APPROXIMATE;
			mstruct = vargs[0];
			mstruct.eval(eo2);
		} else {
			CALCULATOR->endTemporaryStopMessages(true);
		}
	}
	if(!mstruct.isNumber()) {
		if(trig_remove_i(mstruct)) {
			mstruct.transform(CALCULATOR->f_atanh);
			mstruct *= nr_one_i;
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.multiply_nocopy(new MathStructure(180, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.multiply_nocopy(new MathStructure(200, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
				case ANGLE_UNIT_RADIANS: {break;}
				default: {if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
			}
			return 1;
		}
		if(has_predominately_negative_sign(mstruct)) {negate_struct(mstruct); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	if(mstruct.number().isZero()) {
		mstruct.clear();
	} else if(eo.allow_infinite && mstruct.number().isI()) {
		Number nr; nr.setImaginaryPart(nr_plus_inf);
		mstruct = nr;
	} else if(eo.allow_infinite && mstruct.number().isMinusI()) {
		Number nr; nr.setImaginaryPart(nr_minus_inf);
		mstruct = nr;
	} else if(mstruct.number().isPlusInfinity()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(90, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(100, 1, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else if(mstruct.number().isMinusInfinity()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(-90, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(-100, 1, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(-1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(-1, 2, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else if(mstruct.number().isOne()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(45, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(50, 1, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(1, 4, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(1, 4, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else if(mstruct.number().isMinusOne()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {
				mstruct.set(-45, 1, 0);
				break;
			}
			case ANGLE_UNIT_GRADIANS: {
				mstruct.set(-50, 1, 0);
				break;
			}
			case ANGLE_UNIT_RADIANS: {
				mstruct.set(-1, 4, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				break;
			}
			default: {
				mstruct.set(-1, 4, 0);
				mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi));
				if(CALCULATOR->getRadUnit()) {
					mstruct *= CALCULATOR->getRadUnit();
				}
			}
		}
	} else {
		Number nr = mstruct.number();
		if(!nr.atan() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) || (!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
			if(trig_remove_i(mstruct)) {
				mstruct.transform(CALCULATOR->f_atanh);
				mstruct *= nr_one_i;
				switch(eo.parse_options.angle_unit) {
					case ANGLE_UNIT_DEGREES: {mstruct.multiply_nocopy(new MathStructure(180, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
					case ANGLE_UNIT_GRADIANS: {mstruct.multiply_nocopy(new MathStructure(200, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
					case ANGLE_UNIT_RADIANS: {break;}
					default: {if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
				}
				return 1;
			}
			if(has_predominately_negative_sign(mstruct)) {mstruct.number().negate(); mstruct.transform(this); mstruct.negate(); return 1;}
			return -1;
		}
		mstruct = nr;
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {mstruct.multiply_nocopy(new MathStructure(180, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			case ANGLE_UNIT_GRADIANS: {mstruct.multiply_nocopy(new MathStructure(200, 1, 0)); mstruct.divide_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
			case ANGLE_UNIT_RADIANS: {break;}
			default: {if(CALCULATOR->getRadUnit()) {mstruct *= CALCULATOR->getRadUnit();} break;}
		}
	}
	return 1;

}

SinhFunction::SinhFunction() : MathFunction("sinh", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool SinhFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool SinhFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool SinhFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex();}
int SinhFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isFunction() && mstruct.size() == 1) {
		if(mstruct.function() == CALCULATOR->f_asinh) {
			mstruct.setToChild(1, true);
			return 1;
		} else if(mstruct.function() == CALCULATOR->f_acosh && !mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, false, false, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true)) {
			mstruct.setToChild(1);
			MathStructure *mmul = new MathStructure(mstruct);
			mstruct += nr_minus_one;
			mstruct ^= nr_half;
			*mmul += nr_one;
			*mmul ^= nr_half;
			mstruct.multiply_nocopy(mmul);
			return 1;
		} else if(mstruct.function() == CALCULATOR->f_atanh && !mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, false, false, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true)) {
			mstruct.setToChild(1);
			MathStructure *mmul = new MathStructure(mstruct);
			mstruct ^= nr_two;
			mstruct.negate();
			mstruct += nr_one;
			mstruct ^= nr_minus_half;
			mstruct.multiply_nocopy(mmul);
			return 1;
		}
	}
	if(!mstruct.isNumber()) {
		if(trig_remove_i(mstruct)) {mstruct *= CALCULATOR->getRadUnit(); mstruct.transform(CALCULATOR->f_sin); mstruct *= nr_one_i; return 1;}
		if(has_predominately_negative_sign(mstruct)) {negate_struct(mstruct); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	Number nr = mstruct.number();
	if(!nr.sinh() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) || (!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
		if(trig_remove_i(mstruct)) {mstruct *= CALCULATOR->getRadUnit(); mstruct.transform(CALCULATOR->f_sin); mstruct *= nr_one_i; return 1;}
		if(has_predominately_negative_sign(mstruct)) {mstruct.number().negate(); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	mstruct = nr;
	return 1;
}
CoshFunction::CoshFunction() : MathFunction("cosh", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool CoshFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool CoshFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool CoshFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex();}
int CoshFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isFunction() && mstruct.size() == 1) {
		if(mstruct.function() == CALCULATOR->f_acosh) {
			mstruct.setToChild(1, true);
			return 1;
		} else if(mstruct.function() == CALCULATOR->f_asinh) {
			mstruct.setToChild(1);
			mstruct ^= nr_two;
			mstruct += nr_one;
			mstruct ^= nr_half;
			return 1;
		} else if(mstruct.function() == CALCULATOR->f_atanh) {
			mstruct.setToChild(1);
			mstruct ^= nr_two;
			mstruct.negate();
			mstruct += nr_one;
			mstruct ^= nr_minus_half;
			return 1;
		}
	}
	if(!mstruct.isNumber()) {
		if(trig_remove_i(mstruct)) {mstruct *= CALCULATOR->getRadUnit(); mstruct.transform(CALCULATOR->f_cos); return 1;}
		if(has_predominately_negative_sign(mstruct)) {negate_struct(mstruct); return -1;}
		return -1;
	}
	Number nr = mstruct.number();
	if(!nr.cosh() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) || (!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
		if(trig_remove_i(mstruct)) {mstruct *= CALCULATOR->getRadUnit(); mstruct.transform(CALCULATOR->f_cos); return 1;}
		if(has_predominately_negative_sign(mstruct)) mstruct.number().negate();
		return -1;
	}
	mstruct = nr;
	return 1;
}
TanhFunction::TanhFunction() : MathFunction("tanh", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool TanhFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool TanhFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool TanhFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex();}
int TanhFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isFunction() && mstruct.size() == 1) {
		if(mstruct.function() == CALCULATOR->f_atanh) {
			mstruct.setToChild(1, true);
			return 1;
		} else if(mstruct.function() == CALCULATOR->f_asinh && !mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, false, false, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true)) {
			mstruct.setToChild(1);
			MathStructure *mmul = new MathStructure(mstruct);
			mstruct ^= nr_two;
			mstruct += nr_one;
			mstruct ^= nr_minus_half;
			mstruct.multiply_nocopy(mmul);
			return 1;
		} else if(mstruct.function() == CALCULATOR->f_acosh && !mstruct[0].containsInterval(eo.approximation == APPROXIMATION_EXACT, false, false, eo.approximation == APPROXIMATION_EXACT ? 1 : 0, true)) {
			mstruct.setToChild(1);
			MathStructure *mmul = new MathStructure(mstruct);
			MathStructure *mmul2 = new MathStructure(mstruct);
			*mmul2 ^= nr_minus_one;
			mstruct += nr_minus_one;
			mstruct ^= nr_half;
			*mmul += nr_one;
			*mmul ^= nr_half;
			mstruct.multiply_nocopy(mmul);
			mstruct.multiply_nocopy(mmul2);
			return 1;
		}
	}
	if(!mstruct.isNumber()) {
		if(trig_remove_i(mstruct)) {mstruct *= CALCULATOR->getRadUnit(); mstruct.transform(CALCULATOR->f_tan); mstruct *= nr_one_i; return 1;}
		if(has_predominately_negative_sign(mstruct)) {negate_struct(mstruct); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	Number nr = mstruct.number();
	if(!nr.tanh() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) || (!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
		if(has_predominately_negative_sign(mstruct)) {mstruct.number().negate(); mstruct.transform(this); mstruct.negate(); return 1;}
		if(trig_remove_i(mstruct)) {mstruct *= CALCULATOR->getRadUnit(); mstruct.transform(CALCULATOR->f_tan); mstruct *= nr_one_i; return 1;}
		return -1;
	}
	mstruct = nr;
	return 1;
}
AsinhFunction::AsinhFunction() : MathFunction("asinh", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool AsinhFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
bool AsinhFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool AsinhFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex();}
int AsinhFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(!mstruct.isNumber()) {
		if(has_predominately_negative_sign(mstruct)) {negate_struct(mstruct); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	Number nr = mstruct.number();
	if(!nr.asinh() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) || (!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
		if(has_predominately_negative_sign(mstruct)) {mstruct.number().negate(); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	mstruct = nr;
	return 1;
}
AcoshFunction::AcoshFunction() : MathFunction("acosh", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, false));
}
bool AcoshFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 1 && vargs[0].representsNumber(allow_units);}
int AcoshFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(eo.allow_complex && vargs[0].isZero()) {
		mstruct.set(1, 2, 0);
		mstruct.number() *= nr_one_i;
		mstruct *= CALCULATOR->v_pi;
		return 1;
	} else if(vargs[0].isOne()) {
		mstruct.clear();
		return 1;
	} else if(eo.approximation != APPROXIMATION_APPROXIMATE && eo.allow_complex && vargs[0].number() <= -1) {
		mstruct = nr_one_i;
		mstruct *= CALCULATOR->v_pi;
		mstruct.add_nocopy(new MathStructure(this, &vargs[0], NULL));
		mstruct.last()[0].negate();
		return 1;
	}
	FR_FUNCTION(acosh)
}
AtanhFunction::AtanhFunction() : MathFunction("atanh", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
int AtanhFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(!mstruct.isNumber()) {
		if(has_predominately_negative_sign(mstruct)) {negate_struct(mstruct); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	if(eo.allow_complex && mstruct.number().includesInfinity()) {
		if(mstruct.number().isPlusInfinity() || (!mstruct.number().hasRealPart() && mstruct.number().hasImaginaryPart() && mstruct.number().internalImaginary()->isMinusInfinity())) {
			mstruct = nr_minus_half;
			mstruct *= nr_one_i;
			mstruct *= CALCULATOR->v_pi;
			return true;
		} else if(mstruct.number().isMinusInfinity() || (!mstruct.number().hasRealPart() && mstruct.number().hasImaginaryPart() && mstruct.number().internalImaginary()->isPlusInfinity())) {
			mstruct = nr_half;
			mstruct *= nr_one_i;
			mstruct *= CALCULATOR->v_pi;
			return true;
		}
	} else if(eo.approximation != APPROXIMATION_APPROXIMATE && eo.allow_complex && mstruct.number() > 1) {
		mstruct.set(-1, 2, 0);
		mstruct.number() *= nr_one_i;
		mstruct *= CALCULATOR->v_pi;
		mstruct.add_nocopy(new MathStructure(this, &vargs[0], NULL));
		mstruct.last()[0].inverse();
		return 1;
	} else if(eo.approximation != APPROXIMATION_APPROXIMATE && eo.allow_complex && mstruct.number() < -1) {
		mstruct.set(1, 2, 0);
		mstruct.number() *= nr_one_i;
		mstruct *= CALCULATOR->v_pi;
		mstruct.add_nocopy(new MathStructure(this, &vargs[0], NULL));
		mstruct.last()[0].inverse();
		mstruct.last()[0].negate();
		mstruct.last().negate();
		return 1;
	}
	Number nr = mstruct.number();
	if(!nr.atanh() || (eo.approximation == APPROXIMATION_EXACT && nr.isApproximate() && !mstruct.isApproximate()) || (!eo.allow_complex && nr.isComplex() && !mstruct.number().isComplex()) || (!eo.allow_infinite && nr.includesInfinity() && !mstruct.number().includesInfinity())) {
		if(has_predominately_negative_sign(mstruct)) {mstruct.number().negate(); mstruct.transform(this); mstruct.negate(); return 1;}
		return -1;
	}
	mstruct = nr;
	return 1;
}
Atan2Function::Atan2Function() : MathFunction("atan2", 2) {
	NumberArgument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, true);
	arg->setComplexAllowed(false);
	setArgumentDefinition(1, arg);
	arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, true);
	arg->setComplexAllowed(false);
	setArgumentDefinition(2, arg);
}
bool Atan2Function::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[0].representsNumber() && vargs[1].representsNumber();}
int Atan2Function::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].number().isZero()) {
		if(!vargs[1].number().isNonZero()) return 0;
		if(vargs[1].number().isNegative()) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.set(180, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.set(200, 1, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct.set(CALCULATOR->v_pi); break;}
				default: {mstruct.set(CALCULATOR->v_pi); if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();}
			}
		} else {
			mstruct.clear();
		}
	} else if(vargs[1].number().isZero() && vargs[0].number().isNonZero()) {
		switch(eo.parse_options.angle_unit) {
			case ANGLE_UNIT_DEGREES: {mstruct.set(90, 1, 0); break;}
			case ANGLE_UNIT_GRADIANS: {mstruct.set(100, 1, 0); break;}
			case ANGLE_UNIT_RADIANS: {mstruct.set(CALCULATOR->v_pi); mstruct.multiply(nr_half); break;}
			default: {mstruct.set(CALCULATOR->v_pi); mstruct.multiply(nr_half); if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();}
		}
		if(vargs[0].number().hasNegativeSign()) mstruct.negate();
	} else if(!vargs[1].number().isNonZero()) {
		FR_FUNCTION_2(atan2)
	} else {
		MathStructure new_nr(vargs[0]);
		if(!new_nr.number().divide(vargs[1].number())) return -1;
		if(vargs[1].number().isNegative() && vargs[0].number().isNonZero()) {
			if(vargs[0].number().isNegative()) {
				mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
				switch(eo.parse_options.angle_unit) {
					case ANGLE_UNIT_DEGREES: {mstruct.add(-180); break;}
					case ANGLE_UNIT_GRADIANS: {mstruct.add(-200); break;}
					case ANGLE_UNIT_RADIANS: {mstruct.subtract(CALCULATOR->v_pi); break;}
					default: {MathStructure msub(CALCULATOR->v_pi); if(CALCULATOR->getRadUnit()) msub *= CALCULATOR->getRadUnit(); mstruct.subtract(msub);}
				}
			} else {
				mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
				switch(eo.parse_options.angle_unit) {
					case ANGLE_UNIT_DEGREES: {mstruct.add(180); break;}
					case ANGLE_UNIT_GRADIANS: {mstruct.add(200); break;}
					case ANGLE_UNIT_RADIANS: {mstruct.add(CALCULATOR->v_pi); break;}
					default: {MathStructure madd(CALCULATOR->v_pi); if(CALCULATOR->getRadUnit()) madd *= CALCULATOR->getRadUnit(); mstruct.add(madd);}
				}
			}
		} else {
			mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
		}
	}
	return 1;
}

bool calculate_arg(MathStructure &mstruct, const EvaluationOptions &eo) {

	MathStructure msave;

	if(!mstruct.isNumber()) {
		if(mstruct.isPower() && mstruct[0] == CALCULATOR->v_e && mstruct[1].isNumber() && mstruct[1].number().hasImaginaryPart() && !mstruct[1].number().hasRealPart()) {
			CALCULATOR->beginTemporaryStopMessages();
			CALCULATOR->beginTemporaryEnableIntervalArithmetic();
			Number nr(*mstruct[1].number().internalImaginary());
			nr.add(CALCULATOR->v_pi->get().number());
			nr.divide(CALCULATOR->v_pi->get().number() * 2);
			Number nr_u(nr.upperEndPoint());
			nr = nr.lowerEndPoint();
			CALCULATOR->endTemporaryEnableIntervalArithmetic();
			nr_u.floor();
			nr.floor();
			if(!CALCULATOR->endTemporaryStopMessages() && nr == nr_u) {
				nr.setApproximate(false);
				nr *= 2;
				nr.negate();
				mstruct = mstruct[1].number().imaginaryPart();
				if(!nr.isZero()) {
					mstruct += nr;
					mstruct.last() *= CALCULATOR->v_pi;
				}
				return true;
			}
		}
		if(eo.approximation == APPROXIMATION_EXACT) {
			msave = mstruct;
			if(!test_eval(mstruct, eo)) {
				mstruct = msave;
				return false;
			}
		}
	}
	if(mstruct.isNumber()) {
		if(!mstruct.number().hasImaginaryPart()) {
			return false;
		} else if(!mstruct.number().hasRealPart() && mstruct.number().imaginaryPartIsNonZero()) {
			bool b_neg = mstruct.number().imaginaryPartIsNegative();
			mstruct.set(CALCULATOR->v_pi); mstruct.multiply(nr_half);
			if(b_neg) mstruct.negate();
		} else if(!msave.isZero()) {
			mstruct = msave;
			return false;
		} else if(!mstruct.number().realPartIsNonZero()) {
			return false;
		} else {
			MathStructure new_nr(mstruct.number().imaginaryPart());
			if(!new_nr.number().divide(mstruct.number().realPart())) return false;
			if(mstruct.number().realPartIsNegative()) {
				if(mstruct.number().imaginaryPartIsNegative()) {
					mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
					switch(eo.parse_options.angle_unit) {
						case ANGLE_UNIT_DEGREES: {mstruct.divide_nocopy(new MathStructure(180, 1, 0)); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
						case ANGLE_UNIT_GRADIANS: {mstruct.divide_nocopy(new MathStructure(200, 1, 0)); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
						case ANGLE_UNIT_RADIANS: {break;}
						default: {if(CALCULATOR->getRadUnit()) {mstruct /= CALCULATOR->getRadUnit();} break;}
					}
					mstruct.subtract(CALCULATOR->v_pi);
				} else if(mstruct.number().imaginaryPartIsNonNegative()) {
					mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
					switch(eo.parse_options.angle_unit) {
						case ANGLE_UNIT_DEGREES: {mstruct.divide_nocopy(new MathStructure(180, 1, 0)); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
						case ANGLE_UNIT_GRADIANS: {mstruct.divide_nocopy(new MathStructure(200, 1, 0)); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
						case ANGLE_UNIT_RADIANS: {break;}
						default: {if(CALCULATOR->getRadUnit()) {mstruct /= CALCULATOR->getRadUnit();} break;}
					}
					mstruct.add(CALCULATOR->v_pi);
				} else {
					return false;
				}
			} else {
				mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
				switch(eo.parse_options.angle_unit) {
					case ANGLE_UNIT_DEGREES: {mstruct.divide_nocopy(new MathStructure(180, 1, 0)); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
					case ANGLE_UNIT_GRADIANS: {mstruct.divide_nocopy(new MathStructure(200, 1, 0)); mstruct.multiply_nocopy(new MathStructure(CALCULATOR->v_pi)); break;}
					case ANGLE_UNIT_RADIANS: {break;}
					default: {if(CALCULATOR->getRadUnit()) {mstruct /= CALCULATOR->getRadUnit();} break;}
				}
			}
		}
		return true;
	}
	if(!msave.isZero()) {
		mstruct = msave;
	}
	return false;

}

ArgFunction::ArgFunction() : MathFunction("arg", 1) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
bool ArgFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber(true);}
bool ArgFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber(true);}
bool ArgFunction::representsNonComplex(const MathStructure &vargs, bool) const {return true;}
int ArgFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;

	MathStructure msave;

	arg_test_non_number:
	if(!mstruct.isNumber()) {
		if(mstruct.representsPositive(true)) {
			mstruct.clear();
			return 1;
		}
		if(mstruct.representsNegative(true)) {
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.set(180, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.set(200, 1, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct.set(CALCULATOR->v_pi); break;}
				default: {mstruct.set(CALCULATOR->v_pi); if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();}
			}
			return 1;
		}
		if(!msave.isZero()) {
			mstruct = msave;
			return -1;
		}
		if(mstruct.isMultiplication()) {
			bool b = false;
			for(size_t i = 0; i < mstruct.size();) {
				if(mstruct[i].representsPositive()) {
					mstruct.delChild(i + 1);
					b = true;
				} else {
					if(!mstruct[i].isMinusOne() && mstruct[i].representsNegative()) {
						mstruct[i].set(-1, 1, 0, true);
						b = true;
					}
					i++;
				}
			}
			if(b) {
				if(mstruct.size() == 1) {
					mstruct.setToChild(1);
				} else if(mstruct.size() == 0) {
					mstruct.clear(true);
				}
				mstruct.transform(STRUCT_FUNCTION);
				mstruct.setFunction(this);
				return 1;
			}
		}
		if(mstruct.isPower() && mstruct[0].representsComplex() && mstruct[1].representsInteger()) {
			mstruct.setType(STRUCT_MULTIPLICATION);
			mstruct[0].transform(STRUCT_FUNCTION);
			mstruct[0].setFunction(this);
			return 1;
		}
		if(mstruct.isPower() && mstruct[0] == CALCULATOR->v_e && mstruct[1].isNumber() && mstruct[1].number().hasImaginaryPart() && !mstruct[1].number().hasRealPart()) {
			CALCULATOR->beginTemporaryStopMessages();
			CALCULATOR->beginTemporaryEnableIntervalArithmetic();
			Number nr(*mstruct[1].number().internalImaginary());
			nr.add(CALCULATOR->v_pi->get().number());
			nr.divide(CALCULATOR->v_pi->get().number() * 2);
			Number nr_u(nr.upperEndPoint());
			nr = nr.lowerEndPoint();
			CALCULATOR->endTemporaryEnableIntervalArithmetic();
			nr_u.floor();
			nr.floor();
			if(!CALCULATOR->endTemporaryStopMessages() && nr == nr_u) {
				nr.setApproximate(false);
				nr *= 2;
				nr.negate();
				mstruct = mstruct[1].number().imaginaryPart();
				if(!nr.isZero()) {
					mstruct += nr;
					mstruct.last() *= CALCULATOR->v_pi;
				}
				return true;
			}
		}
		if(eo.approximation == APPROXIMATION_EXACT) {
			msave = mstruct;
			if(!test_eval(mstruct, eo)) {
				mstruct = msave;
				return -1;
			}
		}
	}
	if(mstruct.isNumber()) {
		if(!mstruct.number().hasImaginaryPart()) {
			if(!mstruct.number().isNonZero()) {
				if(!msave.isZero()) mstruct = msave;
				return -1;
			}
			if(mstruct.number().isNegative()) {
				switch(eo.parse_options.angle_unit) {
					case ANGLE_UNIT_DEGREES: {mstruct.set(180, 1, 0); break;}
					case ANGLE_UNIT_GRADIANS: {mstruct.set(200, 1, 0); break;}
					case ANGLE_UNIT_RADIANS: {mstruct.set(CALCULATOR->v_pi); break;}
					default: {mstruct.set(CALCULATOR->v_pi); if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();}
				}
			} else {
				mstruct.clear();
			}
		} else if(!mstruct.number().hasRealPart() && mstruct.number().imaginaryPartIsNonZero()) {
			bool b_neg = mstruct.number().imaginaryPartIsNegative();
			switch(eo.parse_options.angle_unit) {
				case ANGLE_UNIT_DEGREES: {mstruct.set(90, 1, 0); break;}
				case ANGLE_UNIT_GRADIANS: {mstruct.set(100, 1, 0); break;}
				case ANGLE_UNIT_RADIANS: {mstruct.set(CALCULATOR->v_pi); mstruct.multiply(nr_half); break;}
				default: {mstruct.set(CALCULATOR->v_pi); mstruct.multiply(nr_half); if(CALCULATOR->getRadUnit()) mstruct *= CALCULATOR->getRadUnit();}
			}
			if(b_neg) mstruct.negate();
		} else if(!msave.isZero()) {
			mstruct = msave;
			return -1;
		} else if(!mstruct.number().realPartIsNonZero()) {
			FR_FUNCTION(arg)
		} else {
			MathStructure new_nr(mstruct.number().imaginaryPart());
			if(!new_nr.number().divide(mstruct.number().realPart())) return -1;
			if(mstruct.number().realPartIsNegative()) {
				if(mstruct.number().imaginaryPartIsNegative()) {
					mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
					switch(eo.parse_options.angle_unit) {
						case ANGLE_UNIT_DEGREES: {mstruct.add(-180); break;}
						case ANGLE_UNIT_GRADIANS: {mstruct.add(-200); break;}
						case ANGLE_UNIT_RADIANS: {mstruct.subtract(CALCULATOR->v_pi); break;}
						default: {MathStructure msub(CALCULATOR->v_pi); if(CALCULATOR->getRadUnit()) msub *= CALCULATOR->getRadUnit(); mstruct.subtract(msub);}
					}
				} else if(mstruct.number().imaginaryPartIsNonNegative()) {
					mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
					switch(eo.parse_options.angle_unit) {
						case ANGLE_UNIT_DEGREES: {mstruct.add(180); break;}
						case ANGLE_UNIT_GRADIANS: {mstruct.add(200); break;}
						case ANGLE_UNIT_RADIANS: {mstruct.add(CALCULATOR->v_pi); break;}
						default: {MathStructure madd(CALCULATOR->v_pi); if(CALCULATOR->getRadUnit()) madd *= CALCULATOR->getRadUnit(); mstruct.add(madd);}
					}
				} else {
					FR_FUNCTION(arg)
				}
			} else {
				mstruct.set(CALCULATOR->f_atan, &new_nr, NULL);
			}
		}
		return 1;
	}
	if(!msave.isZero()) {
		goto arg_test_non_number;
	}
	return -1;
}

SincFunction::SincFunction() : MathFunction("sinc", 1) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
bool SincFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && (vargs[0].representsNumber() || is_number_angle_value(vargs[0]));}
bool SincFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && (vargs[0].representsReal() || is_real_angle_value(vargs[0]));}
bool SincFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex();}
int SincFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.containsType(STRUCT_UNIT) && CALCULATOR->getRadUnit()) {
		mstruct.convert(CALCULATOR->getRadUnit());
		mstruct /= CALCULATOR->getRadUnit();
	}
	if(mstruct.isZero()) {
		mstruct.set(1, 1, 0, true);
		return 1;
	} else if(mstruct.representsNonZero(true)) {
		bool b = replace_f_interval(mstruct, eo);
		b = replace_intervals_f(mstruct) || b;
		MathStructure *m_sin = new MathStructure(CALCULATOR->f_sin, &mstruct, NULL);
		if(CALCULATOR->getRadUnit()) (*m_sin)[0].multiply(CALCULATOR->getRadUnit());
		mstruct.inverse();
		mstruct.multiply_nocopy(m_sin);
		if(b) mstruct.eval(eo);
		return 1;
	}
	return -1;
}
CisFunction::CisFunction() : MathFunction("cis", 1) {
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
}
int CisFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	if(vargs[0].isVector()) return 0;
	if(vargs[0].contains(CALCULATOR->getRadUnit(), false, true, true) > 0 || vargs[0].contains(CALCULATOR->getDegUnit(), false, true, true) > 0 || vargs[0].contains(CALCULATOR->getGraUnit(), false, true, true) > 0) {
		if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getRadUnit()) {
			mstruct = vargs[0][0];
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getRadUnit()) {
			mstruct = vargs[0][1];
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getDegUnit()) {
			mstruct = vargs[0][0];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 180), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getDegUnit()) {
			mstruct = vargs[0][1];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 180), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][1] == CALCULATOR->getGraUnit()) {
			mstruct = vargs[0][0];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 200), true);
		} else if(vargs[0].isMultiplication() && vargs[0].size() == 2 && vargs[0][0] == CALCULATOR->getGraUnit()) {
			mstruct = vargs[0][1];
			mstruct *= CALCULATOR->v_pi;
			mstruct.multiply(Number(1, 200), true);
		} else {
			mstruct = vargs[0];
			mstruct.convert(CALCULATOR->getRadUnit());
			mstruct /= CALCULATOR->getRadUnit();
		}
	} else {
		mstruct = vargs[0];
	}

	if(mstruct.isVariable() && mstruct.variable() == CALCULATOR->v_pi) {
		mstruct.set(-1, 1, 0, true);
		return 1;
	} else if(mstruct.isMultiplication() && mstruct.size() == 2 && mstruct[0].isInteger() && mstruct[1].isVariable() && mstruct[1].variable() == CALCULATOR->v_pi) {
		if(mstruct[0].number().isEven()) {
			mstruct.set(1, 1, 0, true);
			return 1;
		} else if(mstruct[0].number().isOdd()) {
			mstruct.set(-1, 1, 0, true);
			return 1;
		}
	}

	mstruct *= CALCULATOR->v_i;
	mstruct ^= CALCULATOR->v_e;
	mstruct.swapChildren(1, 2);

	return 1;

}


bool create_interval(MathStructure &mstruct, const MathStructure &m1, const MathStructure &m2) {
	if(m1.contains(CALCULATOR->v_pinf, true) || m2.contains(CALCULATOR->v_pinf, true) || m1.contains(CALCULATOR->v_minf, true) || m2.contains(CALCULATOR->v_minf, true)) {
		MathStructure m1b(m1), m2b(m2);
		m1b.replace(CALCULATOR->v_pinf, nr_plus_inf);
		m2b.replace(CALCULATOR->v_pinf, nr_plus_inf);
		m1b.replace(CALCULATOR->v_minf, nr_minus_inf);
		m2b.replace(CALCULATOR->v_minf, nr_minus_inf);
		return create_interval(mstruct, m1b, m2b);
	}
	if(m1 == m2) {
		mstruct.set(m1, true);
		return 1;
	} else if(m1.isNumber() && m2.isNumber()) {
		Number nr;
		if(!nr.setInterval(m1.number(), m2.number())) return false;
		mstruct.set(nr, true);
		return true;
	} else if(m1.isMultiplication() && m2.isMultiplication() && m1.size() > 1 && m2.size() > 1) {
		size_t i0 = 0, i1 = 0;
		if(m1[0].isNumber()) i0++;
		if(m2[0].isNumber()) i1++;
		if(i0 == 0 && i1 == 0) return false;
		if(m1.size() - i0 != m2.size() - i1) return false;
		for(size_t i = 0; i < m1.size() - i0; i++) {
			if(!m1[i + i0].equals(m2[i + i1], true)) {
				return false;
			}
		}
		Number nr;
		if(!nr.setInterval(i0 == 1 ? m1[0].number() : nr_one, i1 == 1 ? m2[0].number() : nr_one)) return 0;
		mstruct.set(m1, true);
		if(i0 == 1) mstruct.delChild(1, true);
		mstruct *= nr;
		mstruct.evalSort(false);
		return 1;
	} else if(m1.isAddition() && m2.isAddition() && m1.size() > 1 && m2.size() > 1) {
		size_t i0 = 0, i1 = 0;
		if(m1.last().isNumber()) i0++;
		if(m2.last().isNumber()) i1++;
		if(i0 == 0 && i1 == 0) return false;
		if(m1.size() - i0 != m2.size() - i1) return false;
		for(size_t i = 0; i < m1.size() - i0; i++) {
			if(!m1[i].equals(m2[i], true)) {
				return false;
			}
		}
		Number nr;
		if(!nr.setInterval(i0 == 1 ? m1.last().number() : nr_one, i1 == 1 ? m2.last().number() : nr_one)) return false;
		mstruct.set(m1, true);
		if(i0 == 1) mstruct.delChild(mstruct.size(), true);
		mstruct += nr;
		mstruct.evalSort(false);
		return true;
	} else if(m1.isMultiplication() && m1.size() == 2 && m1[0].isNumber() && m2.equals(m1[1], true)) {
		Number nr;
		if(!nr.setInterval(m1[0].number(), nr_one)) return false;
		mstruct.set(nr, true);
		mstruct *= m2;
		return true;
	} else if(m2.isMultiplication() && m2.size() == 2 && m2[0].isNumber() && m1.equals(m2[1], true)) {
		Number nr;
		if(!nr.setInterval(nr_one, m2[0].number())) return false;
		mstruct.set(nr, true);
		mstruct *= m1;
		return true;
	}
	return false;
}

IntervalFunction::IntervalFunction() : MathFunction("interval", 2) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false));
	setArgumentDefinition(2, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false));
}
int IntervalFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(create_interval(mstruct, vargs[0], vargs[1])) return 1;
	MathStructure marg1(vargs[0]);
	marg1.eval(eo);
	MathStructure marg2(vargs[1]);
	marg2.eval(eo);
	if(create_interval(mstruct, marg1, marg2)) return 1;
	return 0;
}
bool IntervalFunction::representsPositive(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[1].representsPositive(allow_units) && vargs[0].representsPositive(allow_units);}
bool IntervalFunction::representsNegative(const MathStructure&vargs, bool allow_units) const {return vargs.size() == 2 && vargs[0].representsNegative(allow_units) && vargs[1].representsNegative(allow_units);}
bool IntervalFunction::representsNonNegative(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[0].representsNonNegative(allow_units) && vargs[1].representsNonNegative(allow_units);}
bool IntervalFunction::representsNonPositive(const MathStructure&vargs, bool allow_units) const {return vargs.size() == 2 && vargs[0].representsNonPositive(allow_units) && vargs[1].representsNonPositive(allow_units);}
bool IntervalFunction::representsInteger(const MathStructure &, bool) const {return false;}
bool IntervalFunction::representsNumber(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[0].representsNumber(allow_units) && vargs[1].representsNumber(allow_units);}
bool IntervalFunction::representsRational(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[0].representsRational(allow_units) && vargs[1].representsRational(allow_units);}
bool IntervalFunction::representsReal(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[0].representsReal(allow_units) && vargs[1].representsReal(allow_units);}
bool IntervalFunction::representsNonComplex(const MathStructure &vargs, bool allow_units) const {return vargs.size() == 2 && vargs[0].representsNonComplex(allow_units) && vargs[1].representsNonComplex(allow_units);}
bool IntervalFunction::representsComplex(const MathStructure&, bool) const {return false;}
bool IntervalFunction::representsNonZero(const MathStructure &vargs, bool allow_units) const {return representsPositive(vargs, allow_units) || representsNegative(vargs, allow_units);}
bool IntervalFunction::representsEven(const MathStructure&, bool) const {return false;}
bool IntervalFunction::representsOdd(const MathStructure&, bool) const {return false;}
bool IntervalFunction::representsUndefined(const MathStructure &vargs) const {return vargs.size() == 2 && (vargs[0].representsUndefined() || vargs[1].representsUndefined());}


bool set_uncertainty(MathStructure &mstruct, MathStructure &munc, const EvaluationOptions &eo = default_evaluation_options, bool do_eval = false) {
	if(munc.isFunction() && munc.function() == CALCULATOR->f_abs && munc.size() == 1) {
		munc.setToChild(1, true);
	}
	test_munc:
	if(munc.isNumber()) {
		if(munc.isZero()) {
			return 1;
		} else if(mstruct.isNumber()) {
			mstruct.number().setUncertainty(munc.number(), eo.interval_calculation == INTERVAL_CALCULATION_NONE);
			mstruct.numberUpdated();
			return 1;
		} else if(mstruct.isAddition()) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				if(mstruct[i].isNumber()) {
					mstruct[i].number().setUncertainty(munc.number(), eo.interval_calculation == INTERVAL_CALCULATION_NONE);
					mstruct[i].numberUpdated();
					mstruct.childUpdated(i + 1);
					return 1;
				}
			}
		}
		mstruct.add(m_zero, true);
		mstruct.last().number().setUncertainty(munc.number(), eo.interval_calculation == INTERVAL_CALCULATION_NONE);
		mstruct.last().numberUpdated();
		mstruct.childUpdated(mstruct.size());
		return 1;
	} else {
		if(munc.isMultiplication()) {
			if(!munc[0].isNumber()) {
				munc.insertChild(m_one, 1);
			}
		} else {
			munc.transform(STRUCT_MULTIPLICATION);
			munc.insertChild(m_one, 1);
		}
		if(munc.isMultiplication()) {
			if(munc.size() == 2) {
				if(mstruct.isMultiplication() && mstruct[0].isNumber() && (munc[1] == mstruct[1] || (munc[1].isFunction() && munc[1].function() == CALCULATOR->f_abs && munc[1].size() == 1 && mstruct[1] == munc[1][0]))) {
					mstruct[0].number().setUncertainty(munc[0].number(), eo.interval_calculation == INTERVAL_CALCULATION_NONE);
					mstruct[0].numberUpdated();
					mstruct.childUpdated(1);
					return 1;
				} else if(mstruct.equals(munc[1]) || (munc[1].isFunction() && munc[1].function() == CALCULATOR->f_abs && munc[1].size() == 1 && mstruct.equals(munc[1][0]))) {
					mstruct.transform(STRUCT_MULTIPLICATION);
					mstruct.insertChild(m_one, 1);
					mstruct[0].number().setUncertainty(munc[0].number(), eo.interval_calculation == INTERVAL_CALCULATION_NONE);
					mstruct[0].numberUpdated();
					mstruct.childUpdated(1);
					return 1;
				}
			} else if(mstruct.isMultiplication()) {
				size_t i2 = 0;
				if(mstruct[0].isNumber()) i2++;
				if(mstruct.size() + 1 - i2 == munc.size()) {
					bool b = true;
					for(size_t i = 1; i < munc.size(); i++, i2++) {
						if(!munc[i].equals(mstruct[i2]) && !(munc[i].isFunction() && munc[i].function() == CALCULATOR->f_abs && munc[i].size() == 1 && mstruct[i2] == munc[i][0])) {
							b = false;
							break;
						}
					}
					if(b) {
						if(!mstruct[0].isNumber()) {
							mstruct.insertChild(m_one, 1);
						}
						mstruct[0].number().setUncertainty(munc[0].number(), eo.interval_calculation == INTERVAL_CALCULATION_NONE);
						mstruct[0].numberUpdated();
						mstruct.childUpdated(1);
						return 1;
					}
				}
			}
			if(do_eval) {
				bool b = false;
				for(size_t i = 0; i < munc.size(); i++) {
					if(munc[i].isFunction() && munc[i].function() == CALCULATOR->f_abs && munc[i].size() == 1) {
						munc[i].setToChild(1);
						b = true;
					}
				}
				if(b) {
					munc.eval(eo);
					goto test_munc;
				}
			}
		}
	}
	return false;
}

UncertaintyFunction::UncertaintyFunction() : MathFunction("uncertainty", 2, 3) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false));
	setArgumentDefinition(2, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false));
	setArgumentDefinition(3, new BooleanArgument());
	setDefaultValue(3, "1");
}
int UncertaintyFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	MathStructure munc(vargs[1]);
	mstruct.eval(eo);
	munc.eval(eo);
	if(vargs[2].number().getBoolean()) {
		if(munc.isNumber() && mstruct.isNumber()) {
			mstruct.number().setRelativeUncertainty(munc.number(), eo.interval_calculation == INTERVAL_CALCULATION_NONE);
			mstruct.numberUpdated();
			return 1;
		}
		mstruct = vargs[0];
		mstruct *= m_one;
		mstruct.last() -= vargs[1];
		mstruct.transform(CALCULATOR->f_interval);
		MathStructure *m2 = new MathStructure(vargs[0]);
		m2->multiply(m_one);
		m2->last() += vargs[1];
		mstruct.addChild_nocopy(m2);
		return 1;
	} else {
		if(set_uncertainty(mstruct, munc, eo, true)) return 1;
		mstruct = vargs[0];
		mstruct -= vargs[1];
		mstruct.transform(CALCULATOR->f_interval);
		MathStructure *m2 = new MathStructure(vargs[0]);
		m2->add(vargs[1]);
		mstruct.addChild_nocopy(m2);
		return 1;
	}
	return 0;
}

RadiansToDefaultAngleUnitFunction::RadiansToDefaultAngleUnitFunction() : MathFunction("radtodef", 1) {
}
int RadiansToDefaultAngleUnitFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	switch(eo.parse_options.angle_unit) {
		case ANGLE_UNIT_DEGREES: {
			mstruct *= 180;
	    		mstruct /= CALCULATOR->v_pi;
			break;
		}
		case ANGLE_UNIT_GRADIANS: {
			mstruct *= 200;
	    		mstruct /= CALCULATOR->v_pi;
			break;
		}
		case ANGLE_UNIT_RADIANS: {
			break;
		}
		default: {
			if(CALCULATOR->getRadUnit()) {
				mstruct *= CALCULATOR->getRadUnit();
			}
		}
	}
	return 1;
}

TotalFunction::TotalFunction() : MathFunction("total", 1) {
	setArgumentDefinition(1, new VectorArgument(""));
}
int TotalFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct.clear();
	for(size_t index = 0; index < vargs[0].size(); index++) {
		if(CALCULATOR->aborted()) return 0;
		mstruct.calculateAdd(vargs[0][index], eo);
	}
	return 1;
}
PercentileFunction::PercentileFunction() : MathFunction("percentile", 2, 3) {
	setArgumentDefinition(1, new VectorArgument(""));
	NumberArgument *arg = new NumberArgument();
	Number fr;
	arg->setMin(&fr);
	fr.set(100, 1, 0);
	arg->setMax(&fr);
	arg->setIncludeEqualsMin(true);
	arg->setIncludeEqualsMax(true);
	setArgumentDefinition(2, arg);
	IntegerArgument *iarg = new IntegerArgument();
	fr.set(1, 1, 0);
	iarg->setMin(&fr);
	fr.set(9, 1, 0);
	iarg->setMax(&fr);
	setArgumentDefinition(3, iarg);
	setDefaultValue(3, "8");
}
int PercentileFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	MathStructure v(vargs[0]);
	if(v.size() == 0) {mstruct.clear(); return 1;}
	MathStructure *mp;
	Number fr100(100, 1, 0);
	int i_variant = vargs[2].number().intValue();
	if(!v.sortVector()) {
		return 0;
	} else {
		Number pfr(vargs[1].number());
		if(pfr == fr100) {
			// Max value
			mstruct = v[v.size() - 1];
			return 1;
		} else if(pfr.isZero()) {
			// Min value
			mstruct = v[0];
			return 1;
		}
		pfr /= 100;
		if(pfr == nr_half) {
			// Median
			if(v.size() % 2 == 1) {
				mstruct = v[v.size() / 2];
			} else {
				mstruct = v[v.size() / 2 - 1];
				mstruct += v[v.size() / 2];
				mstruct *= nr_half;
			}
			return 1;
		}
		// Method numbers as in R
		switch(i_variant) {
			case 2: {
				Number ufr(pfr);
				ufr *= (long int) v.countChildren();
				if(ufr.isInteger()) {
					pfr = ufr;
					ufr++;
					mstruct = v[pfr.uintValue() - 1];
					if(ufr.uintValue() > v.size()) return 1;
					mstruct += v[ufr.uintValue() - 1];
					mstruct *= nr_half;
					return 1;
				}
			}
			case 1: {
				pfr *= (long int) v.countChildren();
				pfr.ceil();
				size_t index = pfr.uintValue();
				if(index > v.size()) index = v.size();
				if(index == 0) index = 1;
				mstruct = v[index - 1];
				return 1;
			}
			case 3: {
				pfr *= (long int) v.countChildren();
				pfr.round();
				size_t index = pfr.uintValue();
				if(index > v.size()) index = v.size();
				if(index == 0) index = 1;
				mstruct = v[index - 1];
				return 1;
			}
			case 4: {pfr *= (long int) v.countChildren(); break;}
			case 5: {pfr *= (long int) v.countChildren(); pfr += nr_half; break;}
			case 6: {pfr *= (long int) v.countChildren() + 1; break;}
			case 7: {pfr *= (long int) v.countChildren() - 1; pfr += 1; break;}
			case 9: {pfr *= Number(v.countChildren() * 4 + 1, 4); pfr += Number(3, 8); break;}
			case 8: {}
			default: {pfr *= Number(v.countChildren() * 3 + 1, 3); pfr += Number(1, 3); break;}
		}
		Number ufr(pfr);
		ufr.ceil();
		Number lfr(pfr);
		lfr.floor();
		pfr -= lfr;
		size_t u_index = ufr.uintValue();
		size_t l_index = lfr.uintValue();
		if(u_index > v.size()) {
			mstruct = v[v.size() - 1];
			return 1;
		}
		if(l_index == 0) {
			mstruct = v[0];
			return 1;
		}
		mp = v.getChild(u_index);
		if(!mp) return 0;
		MathStructure gap(*mp);
		mp = v.getChild(l_index);
		if(!mp) return 0;
		gap -= *mp;
		gap *= pfr;
		mp = v.getChild(l_index);
		if(!mp) return 0;
		mstruct = *mp;
		mstruct += gap;
	}
	return 1;
}
MinFunction::MinFunction() : MathFunction("min", 1) {
	setArgumentDefinition(1, new VectorArgument(""));
}
int MinFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	ComparisonResult cmp;
	const MathStructure *min = NULL;
	vector<const MathStructure*> unsolveds;
	bool b = false;
	for(size_t index = 0; index < vargs[0].size(); index++) {
		if(min == NULL) {
			min = &vargs[0][index];
		} else {
			cmp = min->compare(vargs[0][index]);
			if(cmp == COMPARISON_RESULT_LESS) {
				min = &vargs[0][index];
				b = true;
			} else if(COMPARISON_NOT_FULLY_KNOWN(cmp)) {
				if(CALCULATOR->showArgumentErrors()) {
					CALCULATOR->error(true, _("Unsolvable comparison in %s()."), name().c_str(), NULL);
				}
				unsolveds.push_back(&vargs[0][index]);
			} else {
				b = true;
			}
		}
	}
	if(min) {
		if(unsolveds.size() > 0) {
			if(!b) return 0;
			MathStructure margs; margs.clearVector();
			margs.addChild(*min);
			for(size_t i = 0; i < unsolveds.size(); i++) {
				margs.addChild(*unsolveds[i]);
			}
			mstruct.set(this, &margs, NULL);
			return 1;
		} else {
			mstruct = *min;
			return 1;
		}
	}
	return 0;
}
MaxFunction::MaxFunction() : MathFunction("max", 1) {
	setArgumentDefinition(1, new VectorArgument(""));
}
int MaxFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	ComparisonResult cmp;
	const MathStructure *max = NULL;
	vector<const MathStructure*> unsolveds;
	bool b = false;
	for(size_t index = 0; index < vargs[0].size(); index++) {
		if(max == NULL) {
			max = &vargs[0][index];
		} else {
			cmp = max->compare(vargs[0][index]);
			if(cmp == COMPARISON_RESULT_GREATER) {
				max = &vargs[0][index];
				b = true;
			} else if(COMPARISON_NOT_FULLY_KNOWN(cmp)) {
				if(CALCULATOR->showArgumentErrors()) {
					CALCULATOR->error(true, _("Unsolvable comparison in %s()."), name().c_str(), NULL);
				}
				unsolveds.push_back(&vargs[0][index]);
			} else {
				b = true;
			}
		}
	}
	if(max) {
		if(unsolveds.size() > 0) {
			if(!b) return 0;
			MathStructure margs; margs.clearVector();
			margs.addChild(*max);
			for(size_t i = 0; i < unsolveds.size(); i++) {
				margs.addChild(*unsolveds[i]);
			}
			mstruct.set(this, &margs, NULL);
			return 1;
		} else {
			mstruct = *max;
			return 1;
		}
	}
	return 0;
}
ModeFunction::ModeFunction() : MathFunction("mode", 1) {
	setArgumentDefinition(1, new VectorArgument(""));
}
int ModeFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	if(vargs[0].size() <= 0) {
		return 0;
	}
	size_t n = 0;
	bool b;
	vector<const MathStructure*> vargs_nodup;
	vector<size_t> is;
	const MathStructure *value = NULL;
	for(size_t index_c = 0; index_c < vargs[0].size(); index_c++) {
		b = true;
		for(size_t index = 0; index < vargs_nodup.size(); index++) {
			if(vargs_nodup[index]->equals(vargs[0][index_c])) {
				is[index]++;
				b = false;
				break;
			}
		}
		if(b) {
			vargs_nodup.push_back(&vargs[0][index_c]);
			is.push_back(1);
		}
	}
	for(size_t index = 0; index < is.size(); index++) {
		if(is[index] > n) {
			n = is[index];
			value = vargs_nodup[index];
		}
	}
	if(value) {
		mstruct = *value;
		return 1;
	}
	return 0;
}

RandFunction::RandFunction() : MathFunction("rand", 0, 2) {
	setArgumentDefinition(1, new IntegerArgument());
	setDefaultValue(1, "0");
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
	setDefaultValue(2, "1");
}
int RandFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	size_t n = (size_t) vargs[1].number().uintValue();
	if(n > 1) {mstruct.clearVector(); mstruct.resizeVector(n, m_zero);}
	Number nr;
	for(size_t i = 0; i < n; i++) {
		if(vargs[0].number().isZero() || vargs[0].number().isNegative()) {
			nr.rand();
		} else {
			nr.intRand(vargs[0].number());
			nr++;
		}
		if(n > 1) mstruct[i] = nr;
		else mstruct = nr;
	}
	return 1;
}
bool RandFunction::representsReal(const MathStructure&, bool) const {return true;}
bool RandFunction::representsInteger(const MathStructure &vargs, bool) const {return vargs.size() > 0 && vargs[0].isNumber() && vargs[0].number().isPositive();}
bool RandFunction::representsNonNegative(const MathStructure&, bool) const {return true;}

RandnFunction::RandnFunction() : MathFunction("randnorm", 0, 3) {
	setDefaultValue(1, "0");
	setDefaultValue(2, "1");
	setArgumentDefinition(3, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
	setDefaultValue(3, "1");
}
int RandnFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	size_t n = (size_t) vargs[2].number().uintValue();
	if(n > 1) {mstruct.clearVector(); mstruct.resizeVector(n, m_zero);}
#if MPFR_VERSION_MAJOR < 4
	Number nr_u, nr_v, nr_r2;
	for(size_t i = 0; i < n; i++) {
		do {
			nr_u.rand(); nr_u *= 2; nr_u -= 1;
			nr_v.rand(); nr_v *= 2; nr_v -= 1;
			nr_r2 = (nr_u ^ 2) + (nr_v ^ 2);
		} while(nr_r2 > 1 || nr_r2.isZero());
		Number nr_rsq(nr_r2);
		nr_rsq.ln();
		nr_rsq /= nr_r2;
		nr_rsq *= -2;
		nr_rsq.sqrt();
		nr_u *= nr_rsq;
		if(n > 1) {
			mstruct[i] = nr_u;
			i++;
			if(i < n) {
				nr_v *= nr_rsq;
				mstruct[i] = nr_v;
			}
		} else {
			mstruct = nr_u;
		}
	}
#else
	Number nr;
	for(size_t i = 0; i < n; i++) {
		nr.randn();
		if(n > 1) mstruct[i] = nr;
		else mstruct = nr;
	}
#endif
	if(!vargs[1].isOne()) mstruct *= vargs[1];
	if(!vargs[0].isZero()) mstruct += vargs[0];
	return 1;
}
bool RandnFunction::representsReal(const MathStructure&, bool) const {return true;}
bool RandnFunction::representsNonComplex(const MathStructure&, bool) const {return true;}
bool RandnFunction::representsNumber(const MathStructure&, bool) const {return true;}

RandPoissonFunction::RandPoissonFunction() : MathFunction("randpoisson", 1, 2) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_NONNEGATIVE));
	setDefaultValue(1, "0");
	setArgumentDefinition(2, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
	setDefaultValue(2, "1");
}
int RandPoissonFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	size_t n = (size_t) vargs[2].number().uintValue();
	if(n > 1) {mstruct.clearVector(); mstruct.resizeVector(n, m_zero);}
	Number nr_L(vargs[1].number());
	nr_L.exp();
	Number nr_k, nr_p, nr_u;
	for(size_t i = 0; i < n; i++) {
		nr_k.clear(); nr_p = 1;
		do {
			nr_k++;
			nr_u.rand();
			nr_p *= nr_u;
		} while(nr_p > nr_L);
		nr_k--;
		if(n > 1) mstruct[i] = nr_k;
		else mstruct = nr_k;
	}
	return 1;
}
bool RandPoissonFunction::representsReal(const MathStructure&, bool) const {return true;}
bool RandPoissonFunction::representsInteger(const MathStructure &vargs, bool) const {return true;}
bool RandPoissonFunction::representsNonNegative(const MathStructure&, bool) const {return true;}

int calender_to_id(const string &str) {
	if(str == "1" || equalsIgnoreCase(str, "gregorian") || equalsIgnoreCase(str, _("gregorian"))) return CALENDAR_GREGORIAN;
	if(str == "8" || equalsIgnoreCase(str, "milankovic") || equalsIgnoreCase(str, "milanković") || equalsIgnoreCase(str, _("milankovic"))) return CALENDAR_MILANKOVIC;
	if(str == "7" || equalsIgnoreCase(str, "julian") || equalsIgnoreCase(str, _("julian"))) return CALENDAR_JULIAN;
	if(str == "3" || equalsIgnoreCase(str, "islamic") || equalsIgnoreCase(str, _("islamic"))) return CALENDAR_ISLAMIC;
	if(str == "2" || equalsIgnoreCase(str, "hebrew") || equalsIgnoreCase(str, _("hebrew"))) return CALENDAR_HEBREW;
	if(str == "11" || equalsIgnoreCase(str, "egyptian") || equalsIgnoreCase(str, _("egyptian"))) return CALENDAR_EGYPTIAN;
	if(str == "4" || equalsIgnoreCase(str, "persian") || equalsIgnoreCase(str, _("persian"))) return CALENDAR_PERSIAN;
	if(str == "9" || equalsIgnoreCase(str, "coptic") || equalsIgnoreCase(str, _("coptic"))) return CALENDAR_COPTIC;
	if(str == "10" || equalsIgnoreCase(str, "ethiopian") || equalsIgnoreCase(str, _("ethiopian"))) return CALENDAR_ETHIOPIAN;
	if(str == "5" || equalsIgnoreCase(str, "indian") || equalsIgnoreCase(str, _("indian"))) return CALENDAR_INDIAN;
	if(str == "6" || equalsIgnoreCase(str, "chinese") || equalsIgnoreCase(str, _("chinese"))) return CALENDAR_CHINESE;
	return -1;
}

DateFunction::DateFunction() : MathFunction("date", 1, 4) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG));
	IntegerArgument *iarg = new IntegerArgument();
	iarg->setHandleVector(false);
	Number fr(1, 1, 0);
	iarg->setMin(&fr);
	fr.set(24, 1, 0);
	iarg->setMax(&fr);
	setArgumentDefinition(2, iarg);
	setDefaultValue(2, "1");
	iarg = new IntegerArgument();
	iarg->setHandleVector(false);
	fr.set(1, 1, 0);
	iarg->setMin(&fr);
	fr.set(31, 1, 0);
	iarg->setMax(&fr);
	setDefaultValue(3, "1");
	setArgumentDefinition(3, iarg);
	TextArgument *targ = new TextArgument();
	setArgumentDefinition(4, targ);
	setDefaultValue(4, _("gregorian"));
}
int DateFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	int ct = calender_to_id(vargs[3].symbol());
	if(ct < 0) {
		CALCULATOR->error(true, "Unrecognized calendar.", NULL);
		return 0;
	}
	QalculateDateTime dt;
	if(!calendarToDate(dt, vargs[0].number().lintValue(), vargs[1].number().lintValue(), vargs[2].number().lintValue(), (CalendarSystem) ct)) return 0;
	mstruct.set(dt);
	return 1;
}
DateTimeFunction::DateTimeFunction() : MathFunction("datetime", 1, 6) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG));
	IntegerArgument *iarg = new IntegerArgument();
	iarg->setHandleVector(false);
	Number fr(1, 1, 0);
	iarg->setMin(&fr);
	fr.set(12, 1, 0);
	iarg->setMax(&fr);
	setArgumentDefinition(2, iarg);
	setDefaultValue(2, "1");
	iarg = new IntegerArgument();
	iarg->setHandleVector(false);
	fr.set(1, 1, 0);
	iarg->setMin(&fr);
	fr.set(31, 1, 0);
	iarg->setMax(&fr);
	setDefaultValue(3, "1");
	setArgumentDefinition(3, iarg);
	iarg = new IntegerArgument();
	iarg->setHandleVector(false);
	iarg->setMin(&nr_zero);
	fr.set(23, 1, 0);
	iarg->setMax(&fr);
	setArgumentDefinition(4, iarg);
	setDefaultValue(4, "0");
	iarg = new IntegerArgument();
	iarg->setHandleVector(false);
	iarg->setMin(&nr_zero);
	fr.set(59, 1, 0);
	iarg->setMax(&fr);
	setArgumentDefinition(5, iarg);
	setDefaultValue(5, "0");
	NumberArgument *narg = new NumberArgument();
	narg->setHandleVector(false);
	narg->setMin(&nr_zero);
	fr.set(61, 1, 0);
	narg->setMax(&fr);
	narg->setIncludeEqualsMax(false);
	setArgumentDefinition(6, narg);
	setDefaultValue(6, "0");
}
int DateTimeFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime dt;
	if(!dt.set(vargs[0].number().lintValue(), vargs[1].number().lintValue(), vargs[2].number().lintValue())) return 0;
	if(!vargs[3].isZero() || !vargs[4].isZero() || !vargs[5].isZero()) {
		if(!dt.setTime(vargs[3].number().lintValue(), vargs[4].number().lintValue(), vargs[5].number())) return 0;
	}
	mstruct.set(dt);
	return 1;
}
TimeValueFunction::TimeValueFunction() : MathFunction("timevalue", 0, 1) {
	setArgumentDefinition(1, new DateArgument());
	setDefaultValue(1, "now");
}
int TimeValueFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	Number nr(vargs[0].datetime()->second());
	nr /= 60;
	nr += vargs[0].datetime()->minute();
	nr /= 60;
	nr += vargs[0].datetime()->hour();
	mstruct.set(nr);
	return 1;
}
TimestampFunction::TimestampFunction() : MathFunction("timestamp", 0, 1) {
	setArgumentDefinition(1, new DateArgument());
	setDefaultValue(1, "now");
}
int TimestampFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date(*vargs[0].datetime());
	Number nr(date.timestamp());
	if(nr.isInfinite()) return 0;
	mstruct.set(nr);
	return 1;
}
TimestampToDateFunction::TimestampToDateFunction() : MathFunction("stamptodate", 1) {
	//setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG));
}
int TimestampToDateFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	mstruct.eval(eo);
	if((mstruct.isUnit() && mstruct.unit()->baseUnit() == CALCULATOR->u_second) || (mstruct.isMultiplication() && mstruct.size() >= 2 && mstruct.last().isUnit() && mstruct.last().unit()->baseUnit() == CALCULATOR->u_second)) {
		Unit *u = NULL;
		if(mstruct.isUnit()) {
			u = mstruct.unit();
			mstruct.set(1, 1, 0, true);
		} else {
			u = mstruct.last().unit();
			mstruct.delChild(mstruct.size(), true);
		}
		if(u != CALCULATOR->u_second) {
			u->convertToBaseUnit(mstruct);
			mstruct.eval(eo);
		}
	}
	if(!mstruct.isNumber() || !mstruct.number().isReal() || mstruct.number().isInterval()) return -1;
	QalculateDateTime date;
	if(!date.set(mstruct.number())) return -1;
	mstruct.set(date, true);
	return 1;
}

AddDaysFunction::AddDaysFunction() : MathFunction("addDays", 2) {
	setArgumentDefinition(1, new DateArgument());
	setArgumentDefinition(2, new NumberArgument());
}
int AddDaysFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0];
	if(!mstruct.datetime()->addDays(vargs[1].number())) return 0;
	return 1;
}
AddMonthsFunction::AddMonthsFunction() : MathFunction("addMonths", 2) {
	setArgumentDefinition(1, new DateArgument());
	setArgumentDefinition(2, new NumberArgument());
}
int AddMonthsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0];
	if(!mstruct.datetime()->addMonths(vargs[1].number())) return 0;
	return 1;
}
AddYearsFunction::AddYearsFunction() : MathFunction("addYears", 2) {
	setArgumentDefinition(1, new DateArgument());
	setArgumentDefinition(2, new NumberArgument());
}
int AddYearsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = vargs[0];
	if(!mstruct.datetime()->addYears(vargs[1].number())) return 0;
	return 1;
}

DaysFunction::DaysFunction() : MathFunction("days", 2, 4) {
	setArgumentDefinition(1, new DateArgument());
	setArgumentDefinition(2, new DateArgument());
	IntegerArgument *arg = new IntegerArgument();
	Number integ;
	arg->setMin(&integ);
	integ.set(4, 1, 0);
	arg->setMax(&integ);
	setArgumentDefinition(3, arg);
	setArgumentDefinition(4, new BooleanArgument());
	setDefaultValue(3, "1");
}
int DaysFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date1(*vargs[0].datetime()), date2(*vargs[1].datetime());
	Number days(date1.daysTo(date2, vargs[2].number().intValue(), vargs[3].number().isZero()));
	if(days.isInfinite()) return 0;
	days.abs();
	mstruct.set(days);
	return 1;
}
YearFracFunction::YearFracFunction() : MathFunction("yearfrac", 2, 4) {
	setArgumentDefinition(1, new DateArgument());
	setArgumentDefinition(2, new DateArgument());
	IntegerArgument *arg = new IntegerArgument();
	Number integ;
	arg->setMin(&integ);
	integ.set(4, 1, 0);
	arg->setMax(&integ);
	setArgumentDefinition(3, arg);
	setArgumentDefinition(4, new BooleanArgument());
	setDefaultValue(3, "1");
}
int YearFracFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date1(*vargs[0].datetime()), date2(*vargs[1].datetime());
	Number years(date1.yearsTo(date2, vargs[2].number().intValue(), vargs[3].number().isZero()));
	if(years.isInfinite()) return 0;
	years.abs();
	mstruct.set(years);
	return 1;
}
WeekFunction::WeekFunction() : MathFunction("week", 0, 2) {
	setArgumentDefinition(1, new DateArgument());
	setArgumentDefinition(2, new BooleanArgument());
	setDefaultValue(1, "today");
}
int WeekFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date(*vargs[0].datetime());
	int w = date.week(vargs[1].number().getBoolean());
	if(w < 0) return 0;
	mstruct.set(w, 1, 0);
	return 1;
}
WeekdayFunction::WeekdayFunction() : MathFunction("weekday", 0, 2) {
	setArgumentDefinition(1, new DateArgument());
	setArgumentDefinition(2, new BooleanArgument());
	setDefaultValue(1, "today");
}
int WeekdayFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date(*vargs[0].datetime());
	int w = date.weekday();
	if(w < 0) return 0;
	if(vargs[1].number().getBoolean()) {
		if(w == 7) w = 1;
		else w++;
	}
	mstruct.set(w, 1, 0);
	return 1;
}
YeardayFunction::YeardayFunction() : MathFunction("yearday", 0, 1) {
	setArgumentDefinition(1, new DateArgument());
	setDefaultValue(1, "today");
}
int YeardayFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date(*vargs[0].datetime());
	int yd = date.yearday();
	if(yd < 0) return 0;
	mstruct.set(yd, 1, 0);
	return 1;
}
MonthFunction::MonthFunction() : MathFunction("month", 0, 1) {
	setArgumentDefinition(1, new DateArgument());
	setDefaultValue(1, "today");
}
int MonthFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date(*vargs[0].datetime());
	mstruct.set(date.month(), 1L, 0L);
	return 1;
}
DayFunction::DayFunction() : MathFunction("day", 0, 1) {
	setArgumentDefinition(1, new DateArgument());
	setDefaultValue(1, "today");
}
int DayFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date(*vargs[0].datetime());
	mstruct.set(date.day(), 1L, 0L);
	return 1;
}
YearFunction::YearFunction() : MathFunction("year", 0, 1) {
	setArgumentDefinition(1, new DateArgument());
	setDefaultValue(1, "today");
}
int YearFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	QalculateDateTime date(*vargs[0].datetime());
	mstruct.set(date.year(), 1L, 0L);
	return 1;
}
TimeFunction::TimeFunction() : MathFunction("time", 0) {
}
int TimeFunction::calculate(MathStructure &mstruct, const MathStructure&, const EvaluationOptions&) {
	int hour, min, sec;
	now(hour, min, sec);
	Number tnr(sec, 1, 0);
	tnr /= 60;
	tnr += min;
	tnr /= 60;
	tnr += hour;
	mstruct = tnr;
	return 1;
}
LunarPhaseFunction::LunarPhaseFunction() : MathFunction("lunarphase", 0, 1) {
	setArgumentDefinition(1, new DateArgument());
	setDefaultValue(1, "now");
}
int LunarPhaseFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = lunarPhase(*vargs[0].datetime());
	if(CALCULATOR->aborted()) return 0;
	return 1;
}
NextLunarPhaseFunction::NextLunarPhaseFunction() : MathFunction("nextlunarphase", 1, 2) {
	NumberArgument *arg = new NumberArgument();
	Number fr;
	arg->setMin(&fr);
	fr.set(1, 1, 0);
	arg->setMax(&fr);
	arg->setIncludeEqualsMin(true);
	arg->setIncludeEqualsMax(false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
	setArgumentDefinition(2, new DateArgument());
	setDefaultValue(2, "now");
}
int NextLunarPhaseFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = findNextLunarPhase(*vargs[1].datetime(), vargs[0].number());
	if(CALCULATOR->aborted()) return 0;
	return 1;
}

BinFunction::BinFunction() : MathFunction("bin", 1, 2) {
	setArgumentDefinition(1, new TextArgument());
	setArgumentDefinition(2, new BooleanArgument());
	setDefaultValue(2, "0");
}
int BinFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	ParseOptions po = eo.parse_options;
	po.base = BASE_BINARY;
	po.twos_complement = vargs[1].number().getBoolean();
	CALCULATOR->parse(&mstruct, vargs[0].symbol(), po);
	return 1;
}
OctFunction::OctFunction() : MathFunction("oct", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int OctFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	ParseOptions po = eo.parse_options;
	po.base = BASE_OCTAL;
	CALCULATOR->parse(&mstruct, vargs[0].symbol(), po);
	return 1;
}
DecFunction::DecFunction() : MathFunction("dec", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int DecFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	ParseOptions po = eo.parse_options;
	po.base = BASE_DECIMAL;
	CALCULATOR->parse(&mstruct, vargs[0].symbol(), po);
	return 1;
}
HexFunction::HexFunction() : MathFunction("hex", 1, 2) {
	setArgumentDefinition(1, new TextArgument());
	setArgumentDefinition(2, new BooleanArgument());
	setDefaultValue(2, "0");
}
int HexFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	ParseOptions po = eo.parse_options;
	po.base = BASE_HEXADECIMAL;
	po.hexadecimal_twos_complement = vargs[1].number().getBoolean();
	CALCULATOR->parse(&mstruct, vargs[0].symbol(), po);
	return 1;
}

BaseFunction::BaseFunction() : MathFunction("base", 2, 3) {
	setArgumentDefinition(1, new TextArgument());
	Argument *arg = new Argument();
	arg->setHandleVector(true);
	setArgumentDefinition(2, arg);
	IntegerArgument *arg2 = new IntegerArgument();
	arg2->setMin(&nr_zero);
	arg2->setMax(&nr_three);
	setArgumentDefinition(3, arg2);
	setArgumentDefinition(3, new TextArgument());
	setDefaultValue(3, "0");
}
int BaseFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[1].isVector()) return 0;
	Number nbase;
	int idigits = 0;
	string sdigits;
	if(vargs.size() > 2) sdigits = vargs[2].symbol();
	if(sdigits.empty() || sdigits == "0" || sdigits == "auto") idigits = 0;
	else if(sdigits == "1") idigits = 1;
	else if(sdigits == "2") idigits = 2;
	else if(sdigits == "3" || sdigits == "Unicode" || sdigits == "unicode" || sdigits == "escaped") idigits = 3;
	else idigits = -1;
	if(vargs[1].isNumber() && idigits == 0) {
		nbase = vargs[1].number();
	} else {
		mstruct = vargs[1];
		mstruct.eval(eo);
		if(mstruct.isVector()) return -2;
		if(idigits == 0 && !mstruct.isNumber() && eo.approximation == APPROXIMATION_EXACT) {
			MathStructure mstruct2(mstruct);
			EvaluationOptions eo2 = eo;
			eo2.approximation = APPROXIMATION_TRY_EXACT;
			CALCULATOR->beginTemporaryStopMessages();
			mstruct2.eval(eo2);
			if(mstruct2.isVector() || mstruct2.isNumber()) {
				mstruct = mstruct2;
				CALCULATOR->endTemporaryStopMessages(true);
				if(mstruct.isVector()) return -2;
			} else {
				CALCULATOR->endTemporaryStopMessages();
			}
		}
		if(mstruct.isNumber() && idigits == 0) {
			nbase = mstruct.number();
		} else {
			string number = vargs[0].symbol();
			size_t i_dot = number.length();
			vector<Number> digits;
			bool b_minus = false;
			if(idigits < 0) {
				unordered_map<string, long int> vdigits;
				string schar;
				long int v = 0;
				for(size_t i = 0; i < sdigits.length(); v++) {
					size_t l = 1;
					while(i + l < sdigits.length() && sdigits[i + l] <= 0 && (unsigned char) sdigits[i + l] < 0xC0) l++;
					vdigits[sdigits.substr(i, l)] = v;
					i += l;
				}
				remove_blanks(number);
				i_dot = number.length();
				for(size_t i = 0; i < number.length();) {
					size_t l = 1;
					while(i + l < number.length() && number[i + l] <= 0 && (unsigned char) number[i + l] < 0xC0) l++;
					unordered_map<string, long int>::iterator it = vdigits.find(number.substr(i, l));
					if(it == vdigits.end()) {
						if(l == 1 && (number[i] == CALCULATOR->getDecimalPoint()[0] || (!eo.parse_options.dot_as_separator && number[i] == '.'))) {
							if(i_dot == number.length()) i_dot = digits.size();
						} else {
							CALCULATOR->error(true, _("Character \'%s\' was ignored in the number \"%s\" with base %s."), number.substr(i, l).c_str(), number.c_str(), format_and_print(mstruct).c_str(), NULL);
						}
					} else {
						digits.push_back(it->second);
					}
					i += l;
				}
			} else if(idigits <= 2) {
				remove_blanks(number);
				bool b_case = (idigits == 2);
				i_dot = number.length();
				for(size_t i = 0; i < number.length(); i++) {
					long int c = -1;
					if(number[i] >= '0' && number[i] <= '9') {
						c = number[i] - '0';
					} else if(number[i] >= 'a' && number[i] <= 'z') {
						if(b_case) c = number[i] - 'a' + 36;
						else c = number[i] - 'a' + 10;
					} else if(number[i] >= 'A' && number[i] <= 'Z') {
						c = number[i] - 'A' + 10;
					} else if(number[i] == CALCULATOR->getDecimalPoint()[0] || (!eo.parse_options.dot_as_separator && number[i] == '.')) {
						if(i_dot == number.length()) i_dot = digits.size();
					} else if(number[i] == '-' && digits.empty()) {
						b_minus = !b_minus;
					} else {
						string str_char = number.substr(i, 1);
						while(i + 1 < number.length() && number[i + 1] < 0 && number[i + 1] && (unsigned char) number[i + 1] < 0xC0) {
							i++;
							str_char += number[i];
						}
						CALCULATOR->error(true, _("Character \'%s\' was ignored in the number \"%s\" with base %s."), str_char.c_str(), number.c_str(), format_and_print(mstruct).c_str(), NULL);
					}
					if(c >= 0) {
						digits.push_back(c);
					}
				}
			} else {
				for(size_t i = 0; i < number.length(); i++) {
					long int c = (unsigned char) number[i];
					bool b_esc = false;
					if(number[i] == '\\' && i < number.length() - 1) {
						i++;
						Number nrd;
						if(is_in(NUMBERS, number[i])) {
							size_t i2 = number.find_first_not_of(NUMBERS, i);
							if(i2 == string::npos) i2 = number.length();
							nrd.set(number.substr(i, i2 - i));
							i = i2 - 1;
							b_esc = true;
						} else if(number[i] == 'x' && i < number.length() - 1 && is_in(NUMBERS "ABCDEFabcdef", number[i + 1])) {
							i++;
							size_t i2 = number.find_first_not_of(NUMBERS "ABCDEFabcdef", i);
							if(i2 == string::npos) i2 = number.length();
							ParseOptions po;
							po.base = BASE_HEXADECIMAL;
							nrd.set(number.substr(i, i2 - i), po);
							i = i2 - 1;
							b_esc = true;
						}
						if(digits.empty() && number[i] == (char) -30 && i + 3 < number.length() && number[i + 1] == (char) -120 && number[i + 2] == (char) -110) {
							i += 2;
							b_minus = !b_minus;
							b_esc = true;
						} else if(digits.empty() && number[i] == '-') {
							b_minus = !b_minus;
							b_esc = true;
						} else if(i_dot == number.size() && (number[i] == CALCULATOR->getDecimalPoint()[0] || (!eo.parse_options.dot_as_separator && number[i] == '.'))) {
							i_dot = digits.size();
							b_esc = true;
						} else if(b_esc) {
							digits.push_back(nrd);

						} else if(number[i] != '\\') {
							i--;
						}
					}
					if(!b_esc) {
						if((c & 0x80) != 0) {
							if(c<0xe0) {
								i++;
								if(i >= number.length()) return -2;
								c = ((c & 0x1f) << 6) | (((unsigned char) number[i]) & 0x3f);
							} else if(c<0xf0) {
								i++;
								if(i + 1 >= number.length()) return -2;
								c = (((c & 0xf) << 12) | ((((unsigned char) number[i]) & 0x3f) << 6)|(((unsigned char) number[i + 1]) & 0x3f));
								i++;
							} else {
								i++;
								if(i + 2 >= number.length()) return -2;
								c = ((c & 7) << 18) | ((((unsigned char) number[i]) & 0x3f) << 12) | ((((unsigned char) number[i + 1]) & 0x3f) << 6) | (((unsigned char) number[i + 2]) & 0x3f);
								i += 2;
							}
						}
						digits.push_back(c);
					}
				}
			}
			MathStructure mbase = mstruct;
			mstruct.clear();
			if(i_dot > digits.size()) i_dot = digits.size();
			for(size_t i = 0; i < digits.size(); i++) {
				long int exp = i_dot - 1 - i;
				MathStructure m;
				if(exp != 0) {
					m = mbase;
					m.raise(Number(exp, 1));
					m.multiply(digits[i]);
				} else {
					m.set(digits[i]);
				}
				if(mstruct.isZero()) mstruct = m;
				else mstruct.add(m, true);
			}
			if(b_minus) mstruct.negate();
			return 1;
		}
	}
	ParseOptions po = eo.parse_options;
	if(nbase.isInteger() && nbase >= 2 && nbase <= 36) {
		po.base = nbase.intValue();
		CALCULATOR->parse(&mstruct, vargs[0].symbol(), po);
	} else {
		po.base = BASE_CUSTOM;
		Number cb_save = CALCULATOR->customInputBase();
		CALCULATOR->setCustomInputBase(nbase);
		CALCULATOR->parse(&mstruct, vargs[0].symbol(), po);
		CALCULATOR->setCustomInputBase(cb_save);
	}
	return 1;
}
RomanFunction::RomanFunction() : MathFunction("roman", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int RomanFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].symbol().find_first_not_of("0123456789.:" SIGNS) == string::npos && vargs[0].symbol().find_first_not_of("0" SIGNS) != string::npos) {
		CALCULATOR->parse(&mstruct, vargs[0].symbol(), eo.parse_options);
		PrintOptions po; po.base = BASE_ROMAN_NUMERALS;
		mstruct.eval(eo);
		mstruct.set(mstruct.print(po), true, true);
		return 1;
	}
	ParseOptions po = eo.parse_options;
	po.base = BASE_ROMAN_NUMERALS;
	CALCULATOR->parse(&mstruct, vargs[0].symbol(), po);
	return 1;
}
BijectiveFunction::BijectiveFunction() : MathFunction("bijective", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int BijectiveFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].symbol().find_first_not_of("0123456789.:" SIGNS) == string::npos && vargs[0].symbol().find_first_not_of(SIGNS) != string::npos) {
		CALCULATOR->parse(&mstruct, vargs[0].symbol(), eo.parse_options);
		PrintOptions po; po.base = BASE_BIJECTIVE_26;
		mstruct.eval(eo);
		mstruct.set(mstruct.print(po), true, true);
		return 1;
	}
	ParseOptions po = eo.parse_options;
	po.base = BASE_BIJECTIVE_26;
	CALCULATOR->parse(&mstruct, vargs[0].symbol(), po);
	return 1;
}

AsciiFunction::AsciiFunction() : MathFunction("code", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int AsciiFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	if(vargs[0].symbol().empty()) {
		return false;
	}
	const string &str = vargs[0].symbol();
	mstruct.clear();
	for(size_t i = 0; i < str.length(); i++) {
		long int c = (unsigned char) str[i];
		if((c & 0x80) != 0) {
			if(c<0xe0) {
				i++;
				if(i >= str.length()) return false;
				c = ((c & 0x1f) << 6) | (((unsigned char) str[i]) & 0x3f);
			} else if(c<0xf0) {
				i++;
				if(i + 1 >= str.length()) return false;
				c = (((c & 0xf) << 12) | ((((unsigned char) str[i]) & 0x3f) << 6)|(((unsigned char) str[i + 1]) & 0x3f));
				i++;
			} else {
				i++;
				if(i + 2 >= str.length()) return false;
				c = ((c & 7) << 18) | ((((unsigned char) str[i]) & 0x3f) << 12) | ((((unsigned char) str[i + 1]) & 0x3f) << 6) | (((unsigned char) str[i + 2]) & 0x3f);
				i += 2;
			}
		}
		if(mstruct.isZero()) {
			mstruct.set(c, 1L, 0L);
		} else if(mstruct.isVector()) {
			mstruct.addChild(MathStructure(c, 1L, 0L));
		} else {
			mstruct.transform(STRUCT_VECTOR, MathStructure(c, 1L, 0L));
		}
	}
	return 1;
}
CharFunction::CharFunction() : MathFunction("char", 1) {
	IntegerArgument *arg = new IntegerArgument();
	Number fr(32, 1, 0);
	arg->setMin(&fr);
	fr.set(0x10ffff, 1, 0);
	arg->setMax(&fr);
	setArgumentDefinition(1, arg);
}
int CharFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {

	long int v = vargs[0].number().lintValue();
	string str;
	if(v <= 0x7f) {
		str = (char) v;
	} else if(v <= 0x7ff) {
		str = (char) ((v >> 6) | 0xc0);
		str += (char) ((v & 0x3f) | 0x80);
	} else if((v <= 0xd7ff || (0xe000 <= v && v <= 0xffff))) {
		str = (char) ((v >> 12) | 0xe0);
		str += (char) (((v >> 6) & 0x3f) | 0x80);
		str += (char) ((v & 0x3f) | 0x80);
	} else if(0xffff < v && v <= 0x10ffff) {
		str = (char) ((v >> 18) | 0xf0);
		str += (char) (((v >> 12) & 0x3f) | 0x80);
		str += (char) (((v >> 6) & 0x3f) | 0x80);
		str += (char) ((v & 0x3f) | 0x80);
	} else {
		return 0;
	}
	mstruct = str;
	return 1;

}

ConcatenateFunction::ConcatenateFunction() : MathFunction("concatenate", 1, -1) {
	setArgumentDefinition(1, new TextArgument());
	setArgumentDefinition(2, new TextArgument());
}
int ConcatenateFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	string str;
	for(size_t i = 0; i < vargs.size(); i++) {
		str += vargs[i].symbol();
	}
	mstruct = str;
	return 1;
}
LengthFunction::LengthFunction() : MathFunction("len", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int LengthFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	mstruct = (int) vargs[0].symbol().length();
	return 1;
}

extern bool replace_intervals_f(MathStructure &mstruct);
extern bool replace_f_interval(MathStructure &mstruct, const EvaluationOptions &eo);

bool replace_function(MathStructure &m, MathFunction *f1, MathFunction *f2, const EvaluationOptions &eo) {
	bool ret = false;
	if(m.isFunction() && m.function() == f1) {
		m.setFunction(f2);
		while(f2->maxargs() >= 0 && m.size() > (size_t) f2->maxargs()) {
			m.delChild(m.countChildren());
		}
		if(m.size() >= 1) {
			if((f1->getArgumentDefinition(1) && f1->getArgumentDefinition(1)->type() == ARGUMENT_TYPE_ANGLE) && (!f2->getArgumentDefinition(1) || f2->getArgumentDefinition(1)->type() != ARGUMENT_TYPE_ANGLE)) {
				if(m[0].contains(CALCULATOR->getRadUnit(), false, true, true) > 0) m[0] /= CALCULATOR->getRadUnit();
				else if(m[0].contains(CALCULATOR->getDegUnit(), false, true, true) > 0) m[0] /= CALCULATOR->getDegUnit();
				else if(m[0].contains(CALCULATOR->getGraUnit(), false, true, true) > 0) m[0] /= CALCULATOR->getGraUnit();
			} else if((f2->getArgumentDefinition(1) && f2->getArgumentDefinition(1)->type() == ARGUMENT_TYPE_ANGLE) && (!f1->getArgumentDefinition(1) || f1->getArgumentDefinition(1)->type() != ARGUMENT_TYPE_ANGLE)) {
				switch(eo.parse_options.angle_unit) {
					case ANGLE_UNIT_DEGREES: {m[0] *= CALCULATOR->getDegUnit(); break;}
					case ANGLE_UNIT_GRADIANS: {m[0] *= CALCULATOR->getGraUnit(); break;}
					case ANGLE_UNIT_RADIANS: {m[0] *= CALCULATOR->getRadUnit(); break;}
					default: {}
				}
			}
		}
		ret = true;
	}
	for(size_t i = 0; i < m.size(); i++) {
		if(replace_function(m[i], f1, f2, eo)) ret = true;
	}
	return ret;
}
ReplaceFunction::ReplaceFunction() : MathFunction("replace", 3, 4) {
	setArgumentDefinition(4, new BooleanArgument());
	setDefaultValue(4, "0");
}
int ReplaceFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	bool b_evaled = false;
	if(vargs[3].number().getBoolean()) {mstruct.eval(eo); b_evaled = true;}
	if(vargs[1].isVector() && vargs[2].isVector() && vargs[1].size() == vargs[2].size()) {
		for(size_t i = 0; i < vargs[1].size(); i++) {
			if(vargs[1][i].isFunction() && vargs[2][i].isFunction() && vargs[1][i].size() == 0 && vargs[2][i].size() == 0) {
				if(!replace_function(mstruct, vargs[1][i].function(), vargs[2][i].function(), eo)) CALCULATOR->error(false, _("Original value (%s) was not found."), (vargs[1][i].function()->name() + "()").c_str(), NULL);
			} else if(vargs[2][i].containsInterval(true) || vargs[2][i].containsFunction(CALCULATOR->f_interval, true)) {
				MathStructure mv(vargs[2][i]);
				replace_f_interval(mv, eo);
				replace_intervals_f(mv);
				if(!mstruct.replace(vargs[1][i], mv)) CALCULATOR->error(false, _("Original value (%s) was not found."), format_and_print(vargs[1][i]).c_str(), NULL);
			} else {
				if(!mstruct.replace(vargs[1][i], vargs[2][i])) CALCULATOR->error(false, _("Original value (%s) was not found."), format_and_print(vargs[1][i]).c_str(), NULL);
			}
		}
	} else {
		while(true) {
			if(vargs[1].isFunction() && vargs[2].isFunction() && vargs[1].size() == 0 && vargs[2].size() == 0) {
				if(replace_function(mstruct, vargs[1].function(), vargs[2].function(), eo)) break;
			} else if(vargs[2].containsInterval(true) || vargs[2].containsFunction(CALCULATOR->f_interval, true)) {
				MathStructure mv(vargs[2]);
				replace_f_interval(mv, eo);
				replace_intervals_f(mv);
				if(mstruct.replace(vargs[1], mv)) break;
			} else {
				if(mstruct.replace(vargs[1], vargs[2])) break;
			}
			if(b_evaled) {
				CALCULATOR->error(false, _("Original value (%s) was not found."), format_and_print(vargs[1]).c_str(), NULL);
				break;
			} else {
				mstruct.eval();
				b_evaled = true;
			}
		}
	}
	return 1;
}
void remove_nounit(MathStructure &mstruct) {
	if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_stripunits && mstruct.size() == 1) {
		mstruct.setToChild(1, true);
	}
	if(mstruct.isMultiplication() || mstruct.isAddition()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			remove_nounit(mstruct[i]);
		}
	}
}
StripUnitsFunction::StripUnitsFunction() : MathFunction("nounit", 1) {}
int StripUnitsFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	remove_nounit(mstruct);
	mstruct.removeType(STRUCT_UNIT);
	if(mstruct.containsType(STRUCT_UNIT, false, true, true) == 0) return 1;
	if(mstruct.isMultiplication() || mstruct.isAddition()) {
		MathStructure *mleft = NULL;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].containsType(STRUCT_UNIT, false, true, true) == 0) {
				mstruct[i].ref();
				if(mleft) {
					if(mstruct.isMultiplication()) mleft->multiply_nocopy(&mstruct[i], true);
					else mleft->add_nocopy(&mstruct[i], true);
				} else {
					mleft = &mstruct[i];
				}
				mstruct.delChild(i + 1);
			}
		}
		if(mleft) {
			if(mstruct.size() == 0) {
				mstruct.set_nocopy(*mleft);
				mleft->unref();
			} else {
				bool b_multi = mstruct.isMultiplication();
				if(mstruct.size() == 1) {
					mstruct.setType(STRUCT_FUNCTION);
					mstruct.setFunction(this);
				} else {
					mstruct.transform(this);
				}
				if(b_multi) mstruct.multiply_nocopy(mleft, true);
				else mstruct.add_nocopy(mleft, true);
			}
			return 1;
		}
	}
	EvaluationOptions eo2 = eo;
	eo2.sync_units = false;
	eo2.keep_prefixes = true;
	mstruct.eval(eo2);
	remove_nounit(mstruct);
	mstruct.removeType(STRUCT_UNIT);
	if(mstruct.containsType(STRUCT_UNIT, false, true, true) == 0) return 1;
	if(mstruct.isMultiplication() || mstruct.isAddition()) {
		MathStructure *mleft = NULL;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].containsType(STRUCT_UNIT, false, true, true) == 0) {
				mstruct[i].ref();
				if(mleft) {
					if(mstruct.isMultiplication()) mleft->multiply_nocopy(&mstruct[i], true);
					else mleft->add_nocopy(&mstruct[i], true);
				} else {
					mleft = &mstruct[i];
				}
				mstruct.delChild(i + 1);
			}
		}
		if(mleft) {
			if(mstruct.size() == 0) {
				mstruct.set_nocopy(*mleft);
				mleft->unref();
			} else {
				bool b_multi = mstruct.isMultiplication();
				if(mstruct.size() == 1) {
					mstruct.setType(STRUCT_FUNCTION);
					mstruct.setFunction(this);
				} else {
					mstruct.transform(this);
				}
				if(b_multi) mstruct.multiply_nocopy(mleft, true);
				else mstruct.add_nocopy(mleft, true);
			}
			return 1;
		}
	}
	return -1;
}

ErrorFunction::ErrorFunction() : MathFunction("error", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int ErrorFunction::calculate(MathStructure&, const MathStructure &vargs, const EvaluationOptions&) {
	CALCULATOR->error(true, vargs[0].symbol().c_str(), NULL);
	return 1;
}
WarningFunction::WarningFunction() : MathFunction("warning", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int WarningFunction::calculate(MathStructure&, const MathStructure &vargs, const EvaluationOptions&) {
	CALCULATOR->error(false, vargs[0].symbol().c_str(), NULL);
	return 1;
}
MessageFunction::MessageFunction() : MathFunction("message", 1) {
	setArgumentDefinition(1, new TextArgument());
}
int MessageFunction::calculate(MathStructure&, const MathStructure &vargs, const EvaluationOptions&) {
	CALCULATOR->message(MESSAGE_INFORMATION, vargs[0].symbol().c_str(), NULL);
	return 1;
}

GenerateVectorFunction::GenerateVectorFunction() : MathFunction("genvector", 4, 6) {
	setArgumentDefinition(5, new SymbolicArgument());
	setDefaultValue(5, "undefined");
	setArgumentDefinition(6, new BooleanArgument());
	setDefaultValue(6, "0");
}
int GenerateVectorFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(CALCULATOR->aborted()) return 0;
	if(vargs[5].number().getBoolean()) {
		mstruct = vargs[0].generateVector(vargs[4], vargs[1], vargs[2], vargs[3], NULL, eo);
	} else {
		bool overflow = false;
		int steps = vargs[3].number().intValue(&overflow);
		if(!vargs[3].isNumber() || overflow || steps < 1) {
			CALCULATOR->error(true, _("The number of requested elements in generate vector function must be a positive integer."), NULL);
			return 0;
		}
		mstruct = vargs[0].generateVector(vargs[4], vargs[1], vargs[2], steps, NULL, eo);
	}
	if(CALCULATOR->aborted()) return 0;
	return 1;
}
ForFunction::ForFunction() : MathFunction("for", 7) {
	setArgumentDefinition(2, new SymbolicArgument());
	setArgumentDefinition(7, new SymbolicArgument());
}
int ForFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	mstruct = vargs[4];
	MathStructure mcounter = vargs[0];
	MathStructure mtest;
	MathStructure mcount;
	MathStructure mupdate;
	while(true) {
		mtest = vargs[2];
		mtest.replace(vargs[1], mcounter);
		mtest.eval(eo);
		if(!mtest.isNumber() || CALCULATOR->aborted()) return 0;
		if(!mtest.number().getBoolean()) {
			break;
		}
		if(vargs[5].isComparison() && vargs[5].comparisonType() == COMPARISON_EQUALS && vargs[5][0] == vargs[6]) mupdate = vargs[5][1];
		else mupdate = vargs[5];
		mupdate.replace(vargs[1], mcounter, vargs[6], mstruct);
		mstruct = mupdate;
		mstruct.calculatesub(eo, eo, false);

		if(vargs[3].isComparison() && vargs[3].comparisonType() == COMPARISON_EQUALS && vargs[3][0] == vargs[1]) mcount = vargs[3][1];
		else mcount = vargs[3];
		mcount.replace(vargs[1], mcounter);
		mcounter = mcount;
		mcounter.calculatesub(eo, eo, false);
	}
	return 1;

}
SumFunction::SumFunction() : MathFunction("sum", 3, 4) {
	Argument *arg = new IntegerArgument();
	arg->setHandleVector(false);
	setArgumentDefinition(2, arg);
	arg = new IntegerArgument();
	arg->setHandleVector(false);
	setArgumentDefinition(3, arg);
	setArgumentDefinition(4, new SymbolicArgument());
	setDefaultValue(4, "undefined");
	setCondition("\\z >= \\y");
}
int SumFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	MathStructure m1(vargs[0]);
	EvaluationOptions eo2 = eo;
	eo2.calculate_functions = false;
	eo2.expand = false;
	Number i_nr(vargs[1].number());
	if(eo2.approximation == APPROXIMATION_TRY_EXACT) {
		Number nr(vargs[2].number());
		nr.subtract(i_nr);
		if(nr.isGreaterThan(100)) eo2.approximation = APPROXIMATION_APPROXIMATE;
	}
	CALCULATOR->beginTemporaryStopMessages();
	m1.eval(eo2);
	int im = 0;
	if(CALCULATOR->endTemporaryStopMessages(NULL, &im) > 0 || im > 0) m1 = vargs[0];
	eo2.calculate_functions = eo.calculate_functions;
	eo2.expand = eo.expand;
	mstruct.clear();
	MathStructure mstruct_calc;
	bool started = false;
	while(i_nr.isLessThanOrEqualTo(vargs[2].number())) {
		if(CALCULATOR->aborted()) {
			if(!started) {
				return 0;
			} else if(i_nr != vargs[2].number()) {
				MathStructure mmin(i_nr);
				mstruct.add(MathStructure(this, &vargs[0], &mmin, &vargs[2], &vargs[3], NULL), true);
				return 1;
			}
		}
		mstruct_calc.set(m1);
		mstruct_calc.replace(vargs[3], i_nr);
		mstruct_calc.eval(eo2);
		if(started) {
			mstruct.calculateAdd(mstruct_calc, eo2);
		} else {
			mstruct = mstruct_calc;
			mstruct.calculatesub(eo2, eo2);
			started = true;
		}
		i_nr += 1;
	}
	return 1;

}
ProductFunction::ProductFunction() : MathFunction("product", 3, 4) {
	Argument *arg = new IntegerArgument();
	arg->setHandleVector(false);
	setArgumentDefinition(2, arg);
	arg = new IntegerArgument();
	arg->setHandleVector(false);
	setArgumentDefinition(3, arg);
	setArgumentDefinition(4, new SymbolicArgument());
	setDefaultValue(4, "undefined");
	setCondition("\\z >= \\y");
}
int ProductFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	MathStructure m1(vargs[0]);
	EvaluationOptions eo2 = eo;
	eo2.calculate_functions = false;
	eo2.expand = false;
	Number i_nr(vargs[1].number());
	if(eo2.approximation == APPROXIMATION_TRY_EXACT) {
		Number nr(vargs[2].number());
		nr.subtract(i_nr);
		if(nr.isGreaterThan(100)) eo2.approximation = APPROXIMATION_APPROXIMATE;
	}
	CALCULATOR->beginTemporaryStopMessages();
	m1.eval(eo2);
	int im = 0;
	if(CALCULATOR->endTemporaryStopMessages(NULL, &im) || im > 0) m1 = vargs[0];
	eo2.calculate_functions = eo.calculate_functions;
	eo2.expand = eo.expand;
	mstruct.clear();
	MathStructure mstruct_calc;
	bool started = false;
	while(i_nr.isLessThanOrEqualTo(vargs[2].number())) {
		if(CALCULATOR->aborted()) {
			if(!started) {
				return 0;
			} else if(i_nr != vargs[2].number()) {
				MathStructure mmin(i_nr);
				mstruct.multiply(MathStructure(this, &vargs[0], &mmin, &vargs[2], &vargs[3], NULL), true);
				return 1;
			}
		}
		mstruct_calc.set(m1);
		mstruct_calc.replace(vargs[3], i_nr);
		mstruct_calc.eval(eo2);
		if(started) {
			mstruct.calculateMultiply(mstruct_calc, eo2);
		} else {
			mstruct = mstruct_calc;
			mstruct.calculatesub(eo2, eo2);
			started = true;
		}
		i_nr += 1;
	}
	return 1;

}

bool process_replace(MathStructure &mprocess, const MathStructure &mstruct, const MathStructure &vargs, size_t index);
bool process_replace(MathStructure &mprocess, const MathStructure &mstruct, const MathStructure &vargs, size_t index) {
	if(mprocess == vargs[1]) {
		mprocess = mstruct[index];
		return true;
	}
	if(!vargs[3].isEmptySymbol() && mprocess == vargs[3]) {
		mprocess = (int) index + 1;
		return true;
	}
	if(!vargs[4].isEmptySymbol() && mprocess == vargs[4]) {
		mprocess = vargs[2];
		return true;
	}
	bool b = false;
	for(size_t i = 0; i < mprocess.size(); i++) {
		if(process_replace(mprocess[i], mstruct, vargs, index)) {
			mprocess.childUpdated(i + 1);
			b = true;
		}
	}
	return b;
}

ProcessFunction::ProcessFunction() : MathFunction("process", 3, 5) {
	setArgumentDefinition(2, new SymbolicArgument());
	setArgumentDefinition(3, new VectorArgument());
	setArgumentDefinition(4, new SymbolicArgument());
	setDefaultValue(4, "\"\"");
	setArgumentDefinition(5, new SymbolicArgument());
	setDefaultValue(5, "\"\"");
}
int ProcessFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {

	mstruct = vargs[2];
	MathStructure mprocess;
	for(size_t index = 0; index < mstruct.size(); index++) {
		mprocess = vargs[0];
		process_replace(mprocess, mstruct, vargs, index);
		mstruct[index] = mprocess;
	}
	return 1;

}


bool process_matrix_replace(MathStructure &mprocess, const MathStructure &mstruct, const MathStructure &vargs, size_t rindex, size_t cindex);
bool process_matrix_replace(MathStructure &mprocess, const MathStructure &mstruct, const MathStructure &vargs, size_t rindex, size_t cindex) {
	if(mprocess == vargs[1]) {
		mprocess = mstruct[rindex][cindex];
		return true;
	}
	if(!vargs[3].isEmptySymbol() && mprocess == vargs[3]) {
		mprocess = (int) rindex + 1;
		return true;
	}
	if(!vargs[4].isEmptySymbol() && mprocess == vargs[4]) {
		mprocess = (int) cindex + 1;
		return true;
	}
	if(!vargs[5].isEmptySymbol() && mprocess == vargs[5]) {
		mprocess = vargs[2];
		return true;
	}
	bool b = false;
	for(size_t i = 0; i < mprocess.size(); i++) {
		if(process_matrix_replace(mprocess[i], mstruct, vargs, rindex, cindex)) {
			mprocess.childUpdated(i + 1);
			b = true;
		}
	}
	return b;
}

ProcessMatrixFunction::ProcessMatrixFunction() : MathFunction("processm", 3, 6) {
	setArgumentDefinition(2, new SymbolicArgument());
	setArgumentDefinition(3, new MatrixArgument());
	setArgumentDefinition(4, new SymbolicArgument());
	setDefaultValue(4, "\"\"");
	setArgumentDefinition(5, new SymbolicArgument());
	setDefaultValue(5, "\"\"");
	setArgumentDefinition(6, new SymbolicArgument());
	setDefaultValue(6, "\"\"");
}
int ProcessMatrixFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {

	mstruct = vargs[2];
	MathStructure mprocess;
	for(size_t rindex = 0; rindex < mstruct.size(); rindex++) {
		for(size_t cindex = 0; cindex < mstruct[rindex].size(); cindex++) {
			if(CALCULATOR->aborted()) return 0;
			mprocess = vargs[0];
			process_matrix_replace(mprocess, mstruct, vargs, rindex, cindex);
			mstruct[rindex][cindex] = mprocess;
		}
	}
	return 1;

}

bool csum_replace(MathStructure &mprocess, const MathStructure &mstruct, const MathStructure &vargs, size_t index, const EvaluationOptions &eo2);
bool csum_replace(MathStructure &mprocess, const MathStructure &mstruct, const MathStructure &vargs, size_t index, const EvaluationOptions &eo2) {
	if(mprocess == vargs[4]) {
		mprocess = vargs[6][index];
		return true;
	}
	if(mprocess == vargs[5]) {
		mprocess = mstruct;
		return true;
	}
	if(!vargs[7].isEmptySymbol() && mprocess == vargs[7]) {
		mprocess = (int) index + 1;
		return true;
	}
	if(!vargs[8].isEmptySymbol()) {
		if(mprocess.isFunction() && mprocess.function() == CALCULATOR->f_component && mprocess.size() == 2 && mprocess[1] == vargs[8]) {
			bool b = csum_replace(mprocess[0], mstruct, vargs, index, eo2);
			mprocess[0].eval(eo2);
			if(mprocess[0].isNumber() && mprocess[0].number().isInteger() && mprocess[0].number().isPositive() && mprocess[0].number().isLessThanOrEqualTo(vargs[6].size())) {
				mprocess = vargs[6][mprocess[0].number().intValue() - 1];
				return true;
			}
			return csum_replace(mprocess[1], mstruct, vargs, index, eo2) || b;
		} else if(mprocess == vargs[8]) {
			mprocess = vargs[6];
			return true;
		}
	}
	bool b = false;
	for(size_t i = 0; i < mprocess.size(); i++) {
		if(csum_replace(mprocess[i], mstruct, vargs, index, eo2)) {
			mprocess.childUpdated(i + 1);
			b = true;
		}
	}
	return b;
}
CustomSumFunction::CustomSumFunction() : MathFunction("csum", 7, 9) {
	Argument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SINT);
	setArgumentDefinition(1, arg); //begin
	arg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SINT);
	arg->setHandleVector(false);
	setArgumentDefinition(2, arg); //end
	//3. initial
	//4. function
	setArgumentDefinition(5, new SymbolicArgument()); //x var
	setArgumentDefinition(6, new SymbolicArgument()); //y var
	setArgumentDefinition(7, new VectorArgument());
	setArgumentDefinition(8, new SymbolicArgument()); //i var
	setDefaultValue(8, "\"\"");
	setArgumentDefinition(9, new SymbolicArgument()); //v var
	setDefaultValue(9, "\"\"");
}
int CustomSumFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	int start = vargs[0].number().intValue();
	if(start < 1) start = 1;
	int end = vargs[1].number().intValue();
	int n = vargs[6].countChildren();
	if(start > n) {
		CALCULATOR->error(true, _("Too few elements (%s) in vector (%s required)"), i2s(n).c_str(), i2s(start).c_str(), NULL);
		start = n;
	}
	if(end < 1 || end > n) {
		if(end > n) CALCULATOR->error(true, _("Too few elements (%s) in vector (%s required)"), i2s(n).c_str(), i2s(end).c_str(), NULL);
		end = n;
	} else if(end < start) {
		end = start;
	}

	mstruct = vargs[2];
	MathStructure mexpr(vargs[3]);
	MathStructure mprocess;
	EvaluationOptions eo2 = eo;
	eo2.calculate_functions = false;
	mstruct.eval(eo2);
	for(size_t index = (size_t) start - 1; index < (size_t) end; index++) {
		if(CALCULATOR->aborted()) return 0;
		mprocess = mexpr;
		csum_replace(mprocess, mstruct, vargs, index, eo2);
		mprocess.eval(eo2);
		mstruct = mprocess;
	}
	return 1;

}

FunctionFunction::FunctionFunction() : MathFunction("function", 2) {
	setArgumentDefinition(1, new TextArgument());
	setArgumentDefinition(2, new VectorArgument());
}
int FunctionFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	UserFunction f = new UserFunction("", "Generated MathFunction", vargs[0].symbol());
	MathStructure args = vargs[1];
	mstruct = f.MathFunction::calculate(args, eo);
	if(mstruct.isFunction() && mstruct.function() == &f) mstruct.setUndefined();
	return 1;
}
SelectFunction::SelectFunction() : MathFunction("select", 2, 4) {
	setArgumentDefinition(3, new SymbolicArgument());
	setDefaultValue(3, "undefined");
	setArgumentDefinition(4, new BooleanArgument());
	setDefaultValue(4, "0");
}
int SelectFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	MathStructure mtest;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(!mstruct.isVector()) {
		mtest = vargs[1];
		mtest.replace(vargs[2], mstruct);
		mtest.eval(eo);
		if(!mtest.isNumber() || mtest.number().getBoolean() < 0) {
			CALCULATOR->error(true, _("Comparison failed."), NULL);
			return -1;
		}
		if(mtest.number().getBoolean() == 0) {
			if(vargs[3].number().getBoolean() > 0) {
				CALCULATOR->error(true, _("No matching item found."), NULL);
				return -1;
			}
			mstruct.clearVector();
		}
		return 1;
	}
	for(size_t i = 0; i < mstruct.size();) {
		mtest = vargs[1];
		mtest.replace(vargs[2], mstruct[i]);
		mtest.eval(eo);
		if(!mtest.isNumber() || mtest.number().getBoolean() < 0) {
			CALCULATOR->error(true, _("Comparison failed."), NULL);
			return -1;
		}
		if(mtest.number().getBoolean() == 0) {
			if(vargs[3].number().getBoolean() == 0) {
				mstruct.delChild(i + 1);
			} else {
				i++;
			}
		} else if(vargs[3].number().getBoolean() > 0) {
			MathStructure msave(mstruct[i]);
			mstruct = msave;
			return 1;
		} else {
			i++;
		}
	}
	if(vargs[3].number().getBoolean() > 0) {
		CALCULATOR->error(true, _("No matching item found."), NULL);
		return -1;
	}
	return 1;
}
IFFunction::IFFunction() : MathFunction("if", 3, 4) {
	setArgumentDefinition(4, new BooleanArgument());
	setDefaultValue(4, "0");
}
int IFFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isNumber()) {
		int result = vargs[0].number().getBoolean();
		if(result > 0) {
			mstruct = vargs[1];
		} else if(result == 0 || vargs[3].isOne()) {
			mstruct = vargs[2];
		} else {
			return 0;
		}
		return 1;
	}
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!mstruct[i].isNumber() && vargs[3].isZero()) {
				return -1;
			}
			int result = mstruct[i].number().getBoolean();
			if(result > 0) {
				if(vargs[1].isVector() && vargs[1].size() == vargs[0].size()) {
					mstruct = vargs[1][i];
				} else {
					mstruct = vargs[1];
				}
				return 1;
			} else if(result < 0 && vargs[3].isZero()) {
				return -1;
			}
		}
		mstruct = vargs[2];
		return 1;
	}
	if(mstruct.isNumber()) {
		int result = mstruct.number().getBoolean();
		if(result > 0) {
			mstruct = vargs[1];
		} else if(result == 0 || vargs[3].isOne()) {
			mstruct = vargs[2];
		} else {
			return -1;
		}
		return 1;
	}
	if(vargs[3].isOne()) {
		mstruct = vargs[2];
		return 1;
	}
	return -1;
}

IsNumberFunction::IsNumberFunction() : MathFunction("isNumber", 1) {
}
int IsNumberFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	if(!mstruct.isNumber()) mstruct.eval(eo);
	if(mstruct.isNumber()) {
		mstruct.number().setTrue();
	} else {
		mstruct.clear();
		mstruct.number().setFalse();
	}
	return 1;
}
IS_NUMBER_FUNCTION(IsIntegerFunction, isInteger)
IS_NUMBER_FUNCTION(IsRealFunction, isReal)
IS_NUMBER_FUNCTION(IsRationalFunction, isRational)
REPRESENTS_FUNCTION(RepresentsIntegerFunction, representsInteger)
REPRESENTS_FUNCTION(RepresentsRealFunction, representsReal)
REPRESENTS_FUNCTION(RepresentsRationalFunction, representsRational)
REPRESENTS_FUNCTION(RepresentsNumberFunction, representsNumber)

LoadFunction::LoadFunction() : MathFunction("load", 1, 3) {
	setArgumentDefinition(1, new FileArgument());
	Argument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SINT);
	arg->setHandleVector(false);
	setArgumentDefinition(2, arg);
	setDefaultValue(2, "1");
	setArgumentDefinition(3, new TextArgument());
	setDefaultValue(3, ",");
}
int LoadFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	string delim = vargs[2].symbol();
	if(delim == "tab") {
		delim = "\t";
	}
	if(!CALCULATOR->importCSV(mstruct, vargs[0].symbol().c_str(), vargs[1].number().intValue(), delim)) {
		CALCULATOR->error(true, "Failed to load %s.", vargs[0].symbol().c_str(), NULL);
		return 0;
	}
	return 1;
}
ExportFunction::ExportFunction() : MathFunction("export", 2, 3) {
	setArgumentDefinition(1, new VectorArgument());
	setArgumentDefinition(2, new FileArgument());
	setArgumentDefinition(3, new TextArgument());
	setDefaultValue(3, ",");
}
int ExportFunction::calculate(MathStructure&, const MathStructure &vargs, const EvaluationOptions&) {
	string delim = vargs[2].symbol();
	if(delim == "tab") {
		delim = "\t";
	}
	if(!CALCULATOR->exportCSV(vargs[0], vargs[1].symbol().c_str(), delim)) {
		CALCULATOR->error(true, "Failed to export to %s.", vargs[1].symbol().c_str(), NULL);
		return 0;
	}
	return 1;
}
TitleFunction::TitleFunction() : MathFunction("title", 1) {
	setArgumentDefinition(1, new ExpressionItemArgument());
}
int TitleFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	ExpressionItem *item = CALCULATOR->getExpressionItem(vargs[0].symbol());
	if(!item) {
		CALCULATOR->error(true, _("Object %s does not exist."), vargs[0].symbol().c_str(), NULL);
		return 0;
	} else {
		mstruct = item->title();
	}
	return 1;
}
SaveFunction::SaveFunction() : MathFunction("save", 2, 4) {
	setArgumentDefinition(2, new TextArgument());
	setArgumentDefinition(3, new TextArgument());
	setArgumentDefinition(4, new TextArgument());
	setDefaultValue(3, CALCULATOR->temporaryCategory());
	setDefaultValue(4, "");
}
int SaveFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(!CALCULATOR->variableNameIsValid(vargs[1].symbol())) {
		CALCULATOR->error(true, _("Invalid variable name (%s)."), vargs[1].symbol().c_str(), NULL);
		return -1;
	}
	if(CALCULATOR->variableNameTaken(vargs[1].symbol())) {
		Variable *v = CALCULATOR->getActiveVariable(vargs[1].symbol());
		if(v && v->isLocal() && v->isKnown()) {
			if(!vargs[2].symbol().empty()) v->setCategory(vargs[2].symbol());
			if(!vargs[3].symbol().empty()) v->setTitle(vargs[3].symbol());
			((KnownVariable*) v)->set(mstruct);
			if(v->countNames() == 0) {
				ExpressionName ename(vargs[1].symbol());
				ename.reference = true;
				v->setName(ename, 1);
			} else {
				v->setName(vargs[1].symbol(), 1);
			}
		} else {
			CALCULATOR->error(false, _("A global unit or variable was deactivated. It will be restored after the new variable has been removed."), NULL);
			CALCULATOR->addVariable(new KnownVariable(vargs[2].symbol(), vargs[1].symbol(), mstruct, vargs[3].symbol()));
		}
	} else {
		CALCULATOR->addVariable(new KnownVariable(vargs[2].symbol(), vargs[1].symbol(), mstruct, vargs[3].symbol()));
	}
	CALCULATOR->saveFunctionCalled();
	return 1;
}

RegisterFunction::RegisterFunction() : MathFunction("register", 1) {
	setArgumentDefinition(1, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SIZE));
}
int RegisterFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions&) {
	if(vargs[0].number().isGreaterThan(CALCULATOR->RPNStackSize())) {
		CALCULATOR->error(false, _("Register %s does not exist. Returning zero."), format_and_print(vargs[0]).c_str(), NULL);
		mstruct.clear();
		return 1;
	}
	mstruct.set(*CALCULATOR->getRPNRegister((size_t) vargs[0].number().uintValue()));
	return 1;
}
StackFunction::StackFunction() : MathFunction("stack", 0) {
}
int StackFunction::calculate(MathStructure &mstruct, const MathStructure&, const EvaluationOptions&) {
	mstruct.clearVector();
	for(size_t i = 1; i <= CALCULATOR->RPNStackSize(); i++) {
		mstruct.addChild(*CALCULATOR->getRPNRegister(i));
	}
	return 1;
}

bool replace_diff_x(MathStructure &m, const MathStructure &mfrom, const MathStructure &mto) {
	if(m.equals(mfrom, true, true)) {
		m = mto;
		return true;
	}
	if(m.type() == STRUCT_FUNCTION && m.function() == CALCULATOR->f_diff) {
		if(m.size() >= 4 && m[1] == mfrom && m[3].isUndefined()) {
			m[3] = mto;
			return true;
		}
		return false;
	}
	bool b = false;
	for(size_t i = 0; i < m.size(); i++) {
		if(replace_diff_x(m[i], mfrom, mto)) {
			b = true;
			m.childUpdated(i + 1);
		}
	}
	return b;
}

DeriveFunction::DeriveFunction() : MathFunction("diff", 1, 4) {
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
	Argument *arg = new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE, true, true, INTEGER_TYPE_SINT);
	arg->setHandleVector(false);
	setArgumentDefinition(3, arg);
	setDefaultValue(3, "1");
	setDefaultValue(4, "undefined");
}
int DeriveFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	int i = vargs[2].number().intValue();
	mstruct = vargs[0];
	bool b = false;
	while(i) {
		if(CALCULATOR->aborted()) return 0;
		if(!mstruct.differentiate(vargs[1], eo) && !b) {
			return 0;
		}
		b = true;
		i--;
		if(i > 0) {
			EvaluationOptions eo2 = eo;
			eo2.approximation = APPROXIMATION_EXACT;
			eo2.calculate_functions = false;
			mstruct.eval(eo2);
		}
	}
	if(!vargs[3].isUndefined()) replace_diff_x(mstruct, vargs[1], vargs[3]);
	return 1;
}

liFunction::liFunction() : MathFunction("li", 1) {
	names[0].case_sensitive = true;
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, false));
}
bool liFunction::representsReal(const MathStructure &vargs, bool) const {
	return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative() && ((vargs[0].isNumber() && !COMPARISON_IS_NOT_EQUAL(vargs[0].number().compare(nr_one))) || (vargs[0].isVariable() && vargs[0].variable()->isKnown() && ((KnownVariable*) vargs[0].variable())->get().isNumber() && COMPARISON_IS_NOT_EQUAL(((KnownVariable*) vargs[0].variable())->get().number().compare(nr_one))));
}
bool liFunction::representsNonComplex(const MathStructure &vargs, bool) const {
	return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonNegative();
}
bool liFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber() && ((vargs[0].isNumber() && COMPARISON_IS_NOT_EQUAL(vargs[0].number().compare(nr_one))) || (vargs[0].isVariable() && vargs[0].variable()->isKnown() && ((KnownVariable*) vargs[0].variable())->get().isNumber() && COMPARISON_IS_NOT_EQUAL(((KnownVariable*) vargs[0].variable())->get().number().compare(nr_one))));}
int liFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(logint)
}
LiFunction::LiFunction() : MathFunction("Li", 2) {
	names[0].case_sensitive = true;
	setArgumentDefinition(1, new IntegerArgument());
	Argument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false);
	arg->setHandleVector(true);
	setArgumentDefinition(2, arg);
}
bool LiFunction::representsReal(const MathStructure &vargs, bool) const {
	return vargs.size() == 2 && vargs[0].representsInteger() && vargs[1].representsReal() && (vargs[1].representsNonPositive() || ((vargs[1].isNumber() && vargs[1].number().isLessThanOrEqualTo(1)) || (vargs[1].isVariable() && vargs[1].variable()->isKnown() && ((KnownVariable*) vargs[1].variable())->get().isNumber() && ((KnownVariable*) vargs[1].variable())->get().number().isLessThanOrEqualTo(1)))) && ((vargs[0].representsPositive() || ((vargs[1].isNumber() && COMPARISON_IS_NOT_EQUAL(vargs[1].number().compare(nr_one))) || (vargs[1].isVariable() && vargs[1].variable()->isKnown() && ((KnownVariable*) vargs[1].variable())->get().isNumber() && COMPARISON_IS_NOT_EQUAL(((KnownVariable*) vargs[1].variable())->get().number().compare(nr_one))))));
}
bool LiFunction::representsNonComplex(const MathStructure &vargs, bool) const {
	return vargs.size() == 2 && vargs[0].representsInteger() && vargs[1].representsNonComplex() && (vargs[1].representsNonPositive() || ((vargs[1].isNumber() && vargs[1].number().isLessThanOrEqualTo(1)) || (vargs[1].isVariable() && vargs[1].variable()->isKnown() && ((KnownVariable*) vargs[1].variable())->get().isNumber() && ((KnownVariable*) vargs[1].variable())->get().number().isLessThanOrEqualTo(1))));
}
bool LiFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 2 && vargs[0].representsInteger() && (vargs[0].representsPositive() || (vargs[1].representsNumber() && ((vargs[1].isNumber() && COMPARISON_IS_NOT_EQUAL(vargs[1].number().compare(nr_one))) || (vargs[1].isVariable() && vargs[1].variable()->isKnown() && ((KnownVariable*) vargs[1].variable())->get().isNumber() && COMPARISON_IS_NOT_EQUAL(((KnownVariable*) vargs[1].variable())->get().number().compare(nr_one))))));}
int LiFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[1].isVector()) return 0;
	if(vargs[0].isOne()) {
		mstruct.set(1, 1, 0);
		mstruct -= vargs[1];
		mstruct.transform(CALCULATOR->f_ln);
		mstruct.negate();
		return true;
	} else if(vargs[0].isZero()) {
		mstruct.set(1, 1, 0);
		mstruct -= vargs[1];
		mstruct.inverse();
		mstruct *= vargs[1];
		return true;
	} else if(vargs[0].isMinusOne()) {
		mstruct.set(1, 1, 0);
		mstruct -= vargs[1];
		mstruct.raise(Number(-2, 1));
		mstruct *= vargs[1];
		return true;
	}
	mstruct = vargs[1];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -2;
	if(vargs[0].number().isPositive() && mstruct.isOne()) {
		mstruct = vargs[0];
		mstruct.transform(CALCULATOR->f_zeta);
		return true;
	}
	if(mstruct.isNumber()) {
		Number nr(mstruct.number());
		if(nr.polylog(vargs[0].number()) && (eo.approximation != APPROXIMATION_EXACT || !nr.isApproximate() || vargs[0].isApproximate() || mstruct.isApproximate()) && (eo.allow_complex || !nr.isComplex() || vargs[0].number().isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || vargs[0].number().includesInfinity() || mstruct.number().includesInfinity())) {
			mstruct.set(nr); return 1;
		}
	}
	return -2;
}
EiFunction::EiFunction() : MathFunction("Ei", 1) {
	names[0].case_sensitive = true;
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONZERO, true, false));
}
bool EiFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal() && vargs[0].representsNonZero();}
bool EiFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex();}
bool EiFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber() && vargs[0].representsNonZero();}
int EiFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION(expint)
}

SiFunction::SiFunction() : MathFunction("Si", 1) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool SiFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber();}
bool SiFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool SiFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex(true);}
int SiFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isNumber()) {
		Number nr(mstruct.number());
		if(nr.isPlusInfinity()) {
			mstruct.set(CALCULATOR->v_pi, true);
			mstruct *= nr_half;
			return 1;
		}
		if(nr.isMinusInfinity()) {
			mstruct.set(CALCULATOR->v_pi, true);
			mstruct *= nr_minus_half;
			return 1;
		}
		if(nr.hasImaginaryPart() && !nr.hasRealPart()) {
			mstruct.set(nr.imaginaryPart());
			mstruct.transform(CALCULATOR->f_Shi);
			mstruct *= nr_one_i;
			return 1;
		}
		if(nr.sinint() && (eo.approximation != APPROXIMATION_EXACT || !nr.isApproximate() || mstruct.isApproximate()) && (eo.allow_complex || !nr.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || mstruct.number().includesInfinity())) {
			mstruct.set(nr);
			return 1;
		}
	}
	if(has_predominately_negative_sign(mstruct)) {
		negate_struct(mstruct);
		mstruct.transform(this);
		mstruct.negate();
		return 1;
	}
	return -1;
}
CiFunction::CiFunction() : MathFunction("Ci", 1) {
	names[0].case_sensitive = true;
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool CiFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsPositive();}
bool CiFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber();}
bool CiFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonNegative(true);}
int CiFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isNumber()) {
		if(mstruct.number().isNegative()) {
			if(!eo.allow_complex) return -1;
			mstruct.negate();
			mstruct.transform(this);
			mstruct += CALCULATOR->v_pi;
			mstruct.last() *= nr_one_i;
			return 1;
		}
		Number nr(mstruct.number());
		if(nr.isComplex() && nr.hasImaginaryPart() && !nr.hasRealPart()) {
			mstruct.set(nr.imaginaryPart());
			if(nr.imaginaryPartIsNegative()) mstruct.negate();
			mstruct.transform(CALCULATOR->f_Chi);
			mstruct += CALCULATOR->v_pi;
			mstruct.last() *= nr_half;
			if(nr.imaginaryPartIsPositive()) mstruct.last() *= nr_one_i;
			else mstruct.last() *= nr_minus_i;
			return 1;
		}
		if(nr.cosint() && (eo.approximation != APPROXIMATION_EXACT || !nr.isApproximate() || mstruct.isApproximate()) && (eo.allow_complex || !nr.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || mstruct.number().includesInfinity())) {
			mstruct.set(nr);
			return 1;
		}
	}
	return -1;
}
ShiFunction::ShiFunction() : MathFunction("Shi", 1) {
	names[0].case_sensitive = true;
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool ShiFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsReal();}
bool ShiFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber();}
bool ShiFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonComplex(true);}
int ShiFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isNumber()) {
		Number nr(mstruct.number());
		if(nr.hasImaginaryPart() && !nr.hasRealPart()) {
			mstruct.set(nr.imaginaryPart());
			mstruct.transform(CALCULATOR->f_Si);
			mstruct *= nr_one_i;
			return 1;
		}
		if(nr.sinhint() && (eo.approximation != APPROXIMATION_EXACT || !nr.isApproximate() || mstruct.isApproximate()) && (eo.allow_complex || !nr.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || mstruct.number().includesInfinity())) {
			mstruct.set(nr);
			return 1;
		}
	}
	if(has_predominately_negative_sign(mstruct)) {
		negate_struct(mstruct);
		mstruct.transform(this);
		mstruct.negate();
		return 1;
	}
	return -1;
}
ChiFunction::ChiFunction() : MathFunction("Chi", 1) {
	names[0].case_sensitive = true;
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false));
}
bool ChiFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsPositive();}
bool ChiFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNonNegative();}
bool ChiFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 1 && vargs[0].representsNumber();}
int ChiFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[0].isVector()) return 0;
	mstruct = vargs[0];
	mstruct.eval(eo);
	if(mstruct.isVector()) return -1;
	if(mstruct.isNumber()) {
		if(eo.allow_complex && mstruct.number().isNegative()) {
			if(!eo.allow_complex) return -1;
			mstruct.negate();
			mstruct.transform(this);
			mstruct += CALCULATOR->v_pi;
			mstruct.last() *= nr_one_i;
			return 1;
		}
		Number nr(mstruct.number());
		if(nr.isComplex() && nr.hasImaginaryPart() && !nr.hasRealPart()) {
			mstruct.set(nr.imaginaryPart());
			if(nr.imaginaryPartIsNegative()) mstruct.negate();
			mstruct.transform(CALCULATOR->f_Ci);
			mstruct += CALCULATOR->v_pi;
			mstruct.last() *= nr_half;
			if(nr.imaginaryPartIsPositive()) mstruct.last() *= nr_one_i;
			else mstruct.last() *= nr_minus_i;
			return 1;
		}
		if(nr.coshint() && (eo.approximation != APPROXIMATION_EXACT || !nr.isApproximate() || mstruct.isApproximate()) && (eo.allow_complex || !nr.isComplex() || mstruct.number().isComplex()) && (eo.allow_infinite || !nr.includesInfinity() || mstruct.number().includesInfinity())) {
			mstruct.set(nr);
			return 1;
		}
	}
	return -1;
}

IGammaFunction::IGammaFunction() : MathFunction("igamma", 2) {
	setArgumentDefinition(1, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, false));
	setArgumentDefinition(2, new NumberArgument("", ARGUMENT_MIN_MAX_NONE, true, false));
}
bool IGammaFunction::representsReal(const MathStructure &vargs, bool) const {return vargs.size() == 2 && (vargs[1].representsPositive() || (vargs[0].representsInteger() && vargs[0].representsPositive()));}
bool IGammaFunction::representsNonComplex(const MathStructure &vargs, bool) const {return vargs.size() == 2 && (vargs[1].representsNonNegative() || (vargs[0].representsInteger() && vargs[0].representsNonNegative()));}
bool IGammaFunction::representsNumber(const MathStructure &vargs, bool) const {return vargs.size() == 2 && (vargs[1].representsNonZero() || vargs[0].representsPositive());}
int IGammaFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	FR_FUNCTION_2(igamma)
}

IntegrateFunction::IntegrateFunction() : MathFunction("integrate", 1, 5) {
	Argument *arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
	setDefaultValue(2, "undefined");
	arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(2, arg);
	setDefaultValue(3, "undefined");
	arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(3, arg);
	setArgumentDefinition(4, new SymbolicArgument());
	setDefaultValue(4, "undefined");
	setArgumentDefinition(5, new BooleanArgument());
	setDefaultValue(5, "0");
}

// Type 0: Simpson's rule, 1: trapezoid, 2: Simpson's 3/8, 3: Boole's

bool numerical_integration(const MathStructure &mstruct, Number &nvalue, const MathStructure &x_var, const EvaluationOptions &eo2, const Number &nr_begin, const Number &nr_end, int i_samples, int type = 0) {
	if((type == 0 && i_samples % 2 == 1) || (type == 2 && i_samples % 3 == 2) || (type == 3 && i_samples % 4 == 3)) {
		i_samples++;
	} else if((type == 2 && i_samples % 3 == 1) || (type == 3 && i_samples % 4 == 2)) {
		i_samples += 2;
	} else if(type == 3 && i_samples % 4 == 1) {
		i_samples += 3;
	}
	Number nr_step(nr_end);
	nr_step -= nr_begin;
	nr_step /= i_samples;
	MathStructure m_a = mstruct;
	m_a.replace(x_var, nr_begin);
	m_a.eval(eo2);
	if(!m_a.isNumber()) return false;
	nvalue = m_a.number();
	m_a = mstruct;
	m_a.replace(x_var, nr_end);
	m_a.eval(eo2);
	if(!m_a.isNumber()) return false;
	if(!nvalue.add(m_a.number())) return false;
	if(type == 1 && !nvalue.multiply(nr_half)) return false;
	if(type == 3 && !nvalue.multiply(7)) return false;
	for(int i = 1; i < i_samples; i++) {
		if(CALCULATOR->aborted()) {
			return false;
		}
		Number nr(nr_step);
		nr *= i;
		nr += nr_begin;
		MathStructure m_a(mstruct);
		m_a.replace(x_var, nr);
		m_a.eval(eo2);
		if(!m_a.isNumber()) return false;
		if((type == 0 && i % 2 == 0) || (type == 2 && i % 3 == 0)) {
			if(!m_a.number().multiply(nr_two)) return false;
		} else if(type == 0) {
			if(!m_a.number().multiply(4)) return false;
		} else if(type == 2) {
			if(!m_a.number().multiply(nr_three)) return false;
		} else if(type == 3 && i % 4 == 0) {
			if(!m_a.number().multiply(14)) return false;
		} else if(type == 3 && i % 2 == 0) {
			if(!m_a.number().multiply(12)) return false;
		} else if(type == 3) {
			if(!m_a.number().multiply(32)) return false;
		}
		if(!nvalue.add(m_a.number())) return false;
	}
	if(!nvalue.multiply(nr_step)) return false;
	if(type == 0 && !nvalue.multiply(Number(1, 3))) return false;
	if(type == 2 && !nvalue.multiply(Number(3, 8))) return false;
	if(type == 3 && !nvalue.multiply(Number(2, 45))) return false;
	return true;
}
bool montecarlo(const MathStructure &minteg, Number &nvalue, const MathStructure &x_var, const EvaluationOptions &eo, Number a, Number b, Number n) {
	Number range(b); range -= a;
	MathStructure m;
	Number u;
	nvalue.clear();
	vector<Number> v;
	Number i(1, 1);
	while(i <= n) {
		if(CALCULATOR->aborted()) {
			n = i;
			break;
		}
		u.rand();
		u *= range;
		u += a;
		m = minteg;
		m.replace(x_var, u);
		m.eval(eo);
		if(!m.isNumber() || m.number().includesInfinity()) return false;
		if(!m.number().multiply(range)) return false;
		if(!nvalue.add(m.number())) return false;
		v.push_back(m.number());
		i++;
	}
	if(!nvalue.divide(n)) return false;
	Number var;
	for(size_t i = 0; i < v.size(); i++) {
		if(!v[i].subtract(nvalue) || !v[i].square() || !var.add(v[i])) return false;
	}
	if(!var.divide(n) || !var.sqrt()) return false;
	Number nsqrt(n); if(!nsqrt.sqrt() || !var.divide(nsqrt)) return false;
	nvalue.setUncertainty(var);
	return true;
}
bool has_wide_trig_interval(const MathStructure &m, const MathStructure &x_var, const EvaluationOptions &eo, Number a, Number b) {
	for(size_t i = 0; i < m.size(); i++) {
		if(has_wide_trig_interval(m[i], x_var, eo, a, b)) return true;
	}
	if(m.isFunction() && m.size() == 1 && (m.function() == CALCULATOR->f_sin || m.function() == CALCULATOR->f_cos)) {
		Number nr_interval;
		nr_interval.setInterval(a, b);
		MathStructure mtest(m[0]);
		mtest.replace(x_var, nr_interval);
		CALCULATOR->beginTemporaryStopMessages();
		mtest.eval(eo);
		CALCULATOR->endTemporaryStopMessages();
		return mtest.isMultiplication() && mtest.size() >= 1 && mtest[0].isNumber() && (mtest[0].number().uncertainty().realPart() > 100 || mtest[0].number().uncertainty().imaginaryPart() > 100);
	}
	return false;
}
bool romberg(const MathStructure &minteg, Number &nvalue, const MathStructure &x_var, const EvaluationOptions &eo, Number a, Number b, long int max_steps = -1, long int min_steps = 6, bool safety_measures = true) {

	bool auto_max = max_steps <= 0;
	if(auto_max) max_steps = 22;
	if(min_steps > max_steps) max_steps = min_steps;

	Number R1[max_steps], R2[max_steps];
	Number *Rp = &R1[0], *Rc = &R2[0];
	Number h(b); h -= a;
	Number acc, acc_i, c, ntmp, prevunc, prevunc_i, nunc, nunc_i;

	nvalue.clear();

	MathStructure mf(minteg);
	mf.replace(x_var, a);
	mf.eval(eo);
	if(!mf.isNumber() || mf.number().includesInfinity()) {
		if(!a.setToFloatingPoint()) return false;
		mpfr_nextabove(a.internalLowerFloat());
		mpfr_nextabove(a.internalUpperFloat());
		mf = minteg;
		mf.replace(x_var, a);
		mf.eval(eo);
	}
	if(!mf.isNumber()) return false;
	Rp[0] = mf.number();

	mf = minteg;
	mf.replace(x_var, b);
	mf.eval(eo);
	if(!mf.isNumber() || mf.number().includesInfinity()) {
		if(!b.setToFloatingPoint()) return false;
		mpfr_nextbelow(b.internalLowerFloat());
		mpfr_nextbelow(b.internalUpperFloat());
		mf = minteg;
		mf.replace(x_var, a);
		mf.eval(eo);
	}
	if(!mf.isNumber()) return false;

	if(!Rp[0].add(mf.number()) || !Rp[0].multiply(nr_half) || !Rp[0].multiply(h)) return false;
	
	if(safety_measures && min_steps < 15 && has_wide_trig_interval(minteg, x_var, eo, a, b)) min_steps = (max_steps < 15 ? max_steps : 15);

	for(long int i = 1; i < max_steps; i++) {

		if(CALCULATOR->aborted()) break;

		if(!h.multiply(nr_half)) return false;

		c.clear();

		long int ep = 1 << (i - 1);
		ntmp = a; ntmp += h;
		for(long int j = 1; j <= ep; j++){
			if(CALCULATOR->aborted()) break;
			mf = minteg;
			mf.replace(x_var, ntmp);
			mf.eval(eo);
			if(CALCULATOR->aborted()) break;
			if(!mf.isNumber() || mf.number().includesInfinity()) {
				Number ntmp2(ntmp);
				if(ntmp2.setToFloatingPoint()) return false;
				if(j % 2 == 0) {mpfr_nextabove(ntmp2.internalLowerFloat()); mpfr_nextabove(ntmp2.internalUpperFloat());}
				else {mpfr_nextbelow(ntmp2.internalLowerFloat()); mpfr_nextbelow(ntmp2.internalUpperFloat());}
				mf = minteg;
				mf.replace(x_var, ntmp2);
				mf.eval(eo);
				if(CALCULATOR->aborted()) break;
			}
			if(!mf.isNumber() || !c.add(mf.number())) return false;
			ntmp += h; ntmp += h;
		}
		if(CALCULATOR->aborted()) break;

		Rc[0] = h;
		ntmp = Rp[0];
		if(!ntmp.multiply(nr_half) || !Rc[0].multiply(c) || !Rc[0].add(ntmp)) return false;

		for(long int j = 1; j <= i; ++j){
			if(CALCULATOR->aborted()) break;
			ntmp = 4;
			ntmp ^= j;
			Rc[j] = ntmp;
			ntmp--; ntmp.recip();
			if(!Rc[j].multiply(Rc[j - 1]) || !Rc[j].subtract(Rp[j - 1]) || !Rc[j].multiply(ntmp)) return false;
		}
		if(CALCULATOR->aborted()) break;

		if(i >= min_steps - 1 && !Rp[i - 1].includesInfinity() && !Rc[i].includesInfinity()) {
			if(Rp[i - 1].hasImaginaryPart()) nunc = Rp[i - 1].realPart();
			else nunc = Rp[i - 1];
			if(Rc[i].hasImaginaryPart()) nunc -= Rc[i].realPart();
			else nunc -= Rc[i];
			nunc.abs();
			if(safety_measures) nunc *= 10;
			nunc.intervalToMidValue();
			if(Rp[i - 1].hasImaginaryPart() || Rc[i].hasImaginaryPart()) {
				nunc_i = Rp[i - 1].imaginaryPart();
				nunc_i -= Rc[i].imaginaryPart();
				nunc_i.abs();
			} else {
				nunc_i.clear();
			}
			if(safety_measures) nunc_i *= 10;
			nunc_i.intervalToMidValue();
			long int prec = PRECISION + (auto_max ? 3 : 1);
			if(auto_max) {
				if(i > 10) prec += 10 - ((!prevunc.isZero() || !prevunc_i.isZero()) ? i - 1 : i);
				if(prec < 4) prec = 4;
			}
			acc.set(1, 1, -prec);
			ntmp = Rc[i - 1];
			ntmp.intervalToMidValue();
			if(ntmp.hasImaginaryPart()) {
				if(ntmp.hasRealPart()) acc *= ntmp.realPart();
			} else {
				if(!ntmp.isZero()) acc *= ntmp;
			}
			acc.abs();
			acc.intervalToMidValue();
			nvalue = Rc[i - 1];
			if(nunc <= acc) {
				if(!nunc_i.isZero()) {
					acc_i.set(1, 1, -prec);
					if(ntmp.hasImaginaryPart()) acc_i *= ntmp.imaginaryPart();
					acc_i.abs();
					acc_i.intervalToMidValue();
					if(nunc_i <= acc_i) {
						if(!safety_measures) {
							nunc.setImaginaryPart(nunc_i);
							nvalue.setUncertainty(nunc);
							return true;
						}
						if(!prevunc.isZero() || !prevunc_i.isZero() || (nunc.isZero() && nunc_i.isZero())) {
							if(nunc <= prevunc && nunc_i <= prevunc_i) {
								if(!ntmp.hasRealPart()) prevunc = acc;
								if(!ntmp.hasImaginaryPart()) prevunc.setImaginaryPart(acc_i);
								else prevunc.setImaginaryPart(prevunc_i);
								nvalue.setUncertainty(prevunc);
							} else {
								acc.setImaginaryPart(acc_i);
								nvalue.setUncertainty(acc);
							}
							return true;
						}
						prevunc = nunc;
						prevunc_i = nunc_i;
					} else {
						prevunc.clear();
						prevunc_i.clear();
					}
				} else {
					if(!safety_measures) {
						nvalue.setUncertainty(nunc);
						return true;
					}
					if(!prevunc.isZero() || nunc.isZero()) {
						if(!prevunc_i.isZero()) nunc.setImaginaryPart(prevunc_i);
						if(!ntmp.isZero() && nunc <= prevunc) nvalue.setUncertainty(prevunc);
						else nvalue.setUncertainty(acc);
						return true;
					}
					prevunc = nunc;
					prevunc_i = nunc_i;
				}
			} else {
				prevunc.clear();
				prevunc_i.clear();
			}
		}

		Number *rt = Rp;
		Rp = Rc;
		Rc = rt;
	}
	if(!nunc.isZero() || !nunc_i.isZero()) {
		acc.set(1, 1, auto_max ? -3 : -2);
		ntmp = nvalue;
		ntmp.intervalToMidValue();
		if(ntmp.hasImaginaryPart()) {
			if(ntmp.hasRealPart()) acc *= ntmp.realPart();
		} else {
			if(!ntmp.isZero()) acc *= ntmp;
		}
		acc.abs();
		acc.intervalToMidValue();
		if(nunc > acc) return false;
		if(!ntmp.hasRealPart()) nunc = acc;
		if(!nunc_i.isZero()) {
			acc.set(1, 1, -3);
			if(ntmp.hasImaginaryPart()) acc *= ntmp.imaginaryPart();
			acc.abs();
			acc.intervalToMidValue();
			if(nunc_i > acc) return false;
			if(ntmp.hasImaginaryPart()) nunc.setImaginaryPart(nunc_i);
			else nunc.setImaginaryPart(acc);
		}
		if(safety_measures) nunc *= 10;
		nvalue.setUncertainty(nunc);
		return true;
	}
	return false;
}
int numerical_integration_part(const MathStructure &minteg, const MathStructure &x_var, const MathStructure &merr_pre, const MathStructure &merr_diff, KnownVariable *v, Number &nvalue, EvaluationOptions &eo, const Number &nr, int type = 0, int depth = 0, bool nzerodiff = false) {
	if(CALCULATOR->aborted()) return 0;
	eo.interval_calculation = INTERVAL_CALCULATION_NONE;
	MathStructure merr(merr_pre);
	int i_ret = 1;
	v->set(nr);
	bool do_parts = true;
	/*if(nzerodiff) {
		v->set(nr.lowerEndPoint());
		MathStructure mlow(merr);
		mlow.eval(eo);
		v->set(nr.upperEndPoint());
		merr.eval(eo);
		if(!merr.isNumber() || !mlow.isNumber() || merr.number().includesInfinity() || mlow.number().includesInfinity()) return 0;
		merr.number().setInterval(mlow.number(), merr.number());
		do_parts = false;
	} else if(depth > 1 && !merr_diff.isUndefined()) {
		CALCULATOR->beginTemporaryStopMessages();
		MathStructure mzero(merr_diff);
		mzero.eval(eo);
		if(CALCULATOR->endTemporaryStopMessages() == 0 && mzero.isNumber() && mzero.number().isNonZero() && (!mzero.number().hasImaginaryPart() || mzero.number().internalImaginary()->isNonZero())) {
			v->set(nr.lowerEndPoint());
			MathStructure mlow(merr);
			mlow.eval(eo);
			v->set(nr.upperEndPoint());
			merr.eval(eo);
			if(!merr.isNumber() || !mlow.isNumber() || merr.number().includesInfinity() || mlow.number().includesInfinity()) return 0;
			merr.number().setInterval(mlow.number(), merr.number());
			do_parts = false;
			nzerodiff = true;
		}
	}*/
	if(depth > 0 && do_parts) {
		CALCULATOR->beginTemporaryStopMessages();
		merr.eval(eo);
		do_parts = (CALCULATOR->endTemporaryStopMessages() > 0 || !merr.isNumber() || merr.number().includesInfinity());
	}
	if(do_parts) {
		integrate_parts:
		if(depth > 4) {
			if(numerical_integration(minteg, nvalue, x_var, eo, nr.lowerEndPoint(), nr.upperEndPoint(), 1002, type)) {nvalue.setApproximate(); return -1;}
			return 0;
		}
		vector<Number> parts;
		nr.splitInterval(depth == 0 ? 5 : 5, parts);
		nvalue.clear();
		Number n_i;
		for(size_t i = 0; i < parts.size(); i++) {
			int i_reti = numerical_integration_part(minteg, x_var, merr_pre, merr_diff, v, n_i, eo, parts[i], type, depth + 1, nzerodiff);
			if(i_reti != 0) {
				if(i_reti < i_ret) i_ret = i_reti;
				if(!nvalue.add(n_i)) return false;
			} else {
				return 0;
			}
		}
		return i_ret;
	} else {
		CALCULATOR->beginTemporaryStopIntervalArithmetic();
		eo.interval_calculation = INTERVAL_CALCULATION_NONE;
		Number nr_interval(merr.number());
		Number nr_interval_abs;
		Number nr1(nr_interval.upperEndPoint(true));
		Number nr2(nr_interval.lowerEndPoint(true));
		nr1.abs();
		nr2.abs();
		if(nr1.isGreaterThan(nr2)) nr_interval_abs = nr1;
		else nr_interval_abs = nr2;
		if(merr.number().hasImaginaryPart()) {
			if(merr.number().hasRealPart()) {
				nr1 = merr.number().realPart().upperEndPoint();
				nr2 = merr.number().realPart().lowerEndPoint();
				nr1.abs();
				nr2.abs();
				if(nr1.isGreaterThan(nr2)) nr_interval = nr1;
				else nr_interval = nr2;
				nr1 = merr.number().imaginaryPart().upperEndPoint();
				nr2 = merr.number().imaginaryPart().lowerEndPoint();
				nr1.abs();
				nr2.abs();
				if(nr1.isGreaterThan(nr2)) nr_interval.setImaginaryPart(nr1);
				else nr_interval.setImaginaryPart(nr2);
			} else {
				nr_interval.setImaginaryPart(nr_interval_abs);
			}
		} else {
			nr_interval = nr_interval_abs;
		}
		Number nr_samples(6, 1);
		Number nr_range(nr.upperEndPoint());
		nr_range -= nr.lowerEndPoint();
		int i_run = 0;
		while(true) {
			if(CALCULATOR->aborted()) {CALCULATOR->endTemporaryStopIntervalArithmetic(); return 0;}
			if(numerical_integration(minteg, nvalue, x_var, eo, nr.lowerEndPoint(), nr.upperEndPoint(), nr_samples.intValue(), type)) {
				if(nr.includesInfinity()) {
					CALCULATOR->endTemporaryStopIntervalArithmetic();
					return 0;
				}
				Number nr_prec(nr_range);
				if(type == 0 || type == 2) {
					nr_prec /= nr_samples;
					nr_prec ^= 4;
					nr_prec *= nr_range;
					nr_prec /= (type == 2 ? 80 : 180);
				} else if(type == 1) {
					nr_prec /= nr_samples;
					nr_prec ^= 2;
					nr_prec *= nr_range;
					nr_prec /= 12;
				} else if(type == 3) {
					nr_prec ^= 7;
					//nr_prec *= Number(8, 945); ?
					nr_prec /= (nr_samples ^ 6);
				}
				nr_prec *= nr_interval;
				Number nr_prec_abs(nr_prec);
				nr_prec_abs.abs();
				Number mabs(nvalue);
				mabs.abs();
				Number max_error(1, 1, -(PRECISION + 1 - (i_run * 2)));
				max_error *= mabs;
				if(nr_prec_abs >= max_error) {
					for(int i = 0; ; i++) {
						if(type == 0 || type == 2) {
							nr_samples = nr_range;
							nr_samples ^= 5;
							nr_samples /= (type == 2 ? 80 : 180);
							nr_samples *= nr_interval_abs;
							nr_samples /= max_error;
							nr_samples.root(4);
						} else if(type == 3) {
							nr_samples = nr_range;
							nr_samples ^= 7;
							//nr_samples *= Number(8, 945); ?
							nr_samples *= nr_interval_abs;
							nr_samples /= max_error;
							nr_samples.root(6);
						} else {
							nr_samples = nr_range;
							nr_samples ^= 3;
							nr_samples /= 12;
							nr_samples *= nr_interval_abs;
							nr_samples /= max_error;
							nr_samples.sqrt();
						}
						nr_samples.ceil();
						if(type == 0 && nr_samples.isOdd()) {
							nr_samples++;
						} else if(type == 2 && !nr_samples.isIntegerDivisible(3)) {
							Number nmod(nr_samples);
							nmod.mod(3);
							if(nmod == 1) nr_samples += 2;
							else nr_samples += 1;
						} else if(type == 3 && !nr_samples.isIntegerDivisible(4)) {
							Number nmod(nr_samples);
							nmod.mod(4);
							if(nmod == 1) nr_samples += 3;
							if(nmod == 2) nr_samples += 2;
							else nr_samples += 1;
						}
						if(nr_samples < 10000) break;
						i_run++;
						if(depth <= 3 && PRECISION + 1 - (i_run * 2) < (PRECISION > 10 ? 10 : PRECISION) - depth) goto integrate_parts;
						if(PRECISION + 1 - (i_run * 2) < 5) {
							if(nr_samples < 50000) break;
							if(PRECISION + 1 - (i_run * 2) < 2 - depth) {
								if(numerical_integration(minteg, nvalue, x_var, eo, nr.lowerEndPoint(), nr.upperEndPoint(), 1002, type)) {nvalue.setApproximate(); return -1;}
								return 0;
							}
						}
						max_error *= Number(1, 1, (PRECISION + 1 - ((i_run - 1) * 2)));
						max_error *= Number(1, 1, -(PRECISION + 1 - (i_run * 2)));
					}
				} else {
					CALCULATOR->endTemporaryStopIntervalArithmetic();
					nvalue.setUncertainty(nr_prec);
					return 1;
				}
			} else {
				CALCULATOR->endTemporaryStopIntervalArithmetic();
				return 0;
			}
			i_run++;
		}
	}
	return 0;
}
bool contains_complex(const MathStructure &mstruct) {
	if(mstruct.isNumber()) return mstruct.number().isComplex();
	if(mstruct.isVariable() && mstruct.variable()->isKnown()) return contains_complex(((KnownVariable*) mstruct.variable())->get());
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(contains_complex(mstruct[i])) return true;
	}
	return false;
}

bool check_denominators(const MathStructure &m, const MathStructure &mi, const MathStructure &mx, const EvaluationOptions &eo) {
	if(m.contains(mx, false, true, true) == 0) return true;
	for(size_t i = 0; i < m.size(); i++) {
		if(!check_denominators(m[i], mi, mx, eo)) return false;
	}
	if(m.isPower()) {
		bool b_neg = m[1].representsNegative();
		if(!m[1].representsNonNegative()) {
			if(!m[0].representsNonZero()) {
				EvaluationOptions eo2 = eo;
				eo2.approximation = APPROXIMATION_APPROXIMATE;
				eo2.interval_calculation = INTERVAL_CALCULATION_INTERVAL_ARITHMETIC;
				CALCULATOR->beginTemporaryStopMessages();
				KnownVariable *v = new KnownVariable("", "v", mi);
				MathStructure mpow(m[1]);
				mpow.replace(mx, v, true);
				mpow.eval(eo2);
				b_neg = mpow.representsNegative();
				CALCULATOR->endTemporaryStopMessages();
				v->destroy();
			}
		}
		if(b_neg && !m[0].representsNonZero()) {
			if(m[0].isZero()) return false;
			EvaluationOptions eo2 = eo;
			eo2.approximation = APPROXIMATION_APPROXIMATE;
			eo2.interval_calculation = INTERVAL_CALCULATION_SIMPLE_INTERVAL_ARITHMETIC;
			CALCULATOR->beginTemporaryStopMessages();
			MathStructure mbase(m[0]);
			KnownVariable *v = new KnownVariable("", "v", mi);
			mbase.replace(mx, v, true);
			bool b_multiple = mbase.contains(mx, true) > 0;
			if(b_multiple) mbase.replace(mx, v);
			mbase.eval(eo2);
			CALCULATOR->endTemporaryStopMessages();
			/*if(mbase.isZero()) {
				v->destroy(); return false;
			} else if(!b_multiple && mbase.isNumber()) {
				if(!mbase.number().isNonZero()) {v->destroy(); return false;}
			} else if(!mbase.isNumber() || !mbase.number().isNonZero()) {
				CALCULATOR->beginTemporaryStopMessages();
				eo2.interval_calculation = INTERVAL_CALCULATION_INTERVAL_ARITHMETIC;
				mbase = m[0];
				mbase.replace(mx, v);
				mbase.eval(eo2);
				CALCULATOR->endTemporaryStopMessages();
			}*/
			if(mbase.isZero()) {
				v->destroy(); return false;
			} else if(!b_multiple && mbase.isNumber()) {
				if(!mbase.number().isNonZero()) {v->destroy(); return false;}
			} else if(!mbase.isNumber() || !mbase.number().isNonZero()) {
				CALCULATOR->beginTemporaryStopMessages();
				mbase = m[0];
				eo2.isolate_x = true;
				eo2.isolate_var = &mx;
				eo2.test_comparisons = true;
				eo2.approximation = APPROXIMATION_EXACT;
				mbase.transform(COMPARISON_NOT_EQUALS, m_zero);
				mbase.eval(eo2);
				eo2.approximation = APPROXIMATION_APPROXIMATE;
				eo2.isolate_x = false;
				eo2.isolate_var = NULL;
				if(!mbase.isNumber()) {
					MathStructure mtest(mbase);
					mtest.replace(mx, v);
					mtest.eval(eo2);
					if(mtest.isNumber()) mbase = mtest;
				}
				CALCULATOR->endTemporaryStopMessages();
				if(!mbase.isOne()) {
					if(mbase.isZero()) {v->destroy(); return false;}
					bool b = false;
					if(mbase.isComparison()) {
						CALCULATOR->beginTemporaryStopMessages();
						mbase[0].eval(eo2); mbase[1].eval(eo2);
						CALCULATOR->endTemporaryStopMessages();
					}
					if(mbase.isComparison() && mbase.comparisonType() == COMPARISON_NOT_EQUALS && (mbase[0] == mx || mbase[0].isFunction()) && mbase[1].isNumber() && mi.isNumber()) {
						ComparisonResult cr = COMPARISON_RESULT_UNKNOWN;
						if(mbase[0].isFunction()) {
							MathStructure mfunc(mbase[0]);
							mfunc.replace(mx, v);
							mfunc.eval(eo2);
							if(mfunc.isNumber()) cr = mbase[1].number().compare(mfunc.number());
						} else {
							cr = mbase[1].number().compare(mi.number());
						}
						b = COMPARISON_IS_NOT_EQUAL(cr);
						if(!b && cr != COMPARISON_RESULT_UNKNOWN) {v->destroy(); return false;}
					} else if(mbase.isLogicalAnd()) {
						for(size_t i = 0; i < mbase.size(); i++) {
							if(mbase[i].isComparison()) {mbase[i][0].eval(eo2); mbase[i][1].eval(eo2);}
							if(mbase[i].isComparison() && mbase[i].comparisonType() == COMPARISON_NOT_EQUALS && (mbase[i][0] == mx || mbase[i][0].isFunction()) && mbase[i][1].isNumber() && mi.isNumber()) {
								ComparisonResult cr = COMPARISON_RESULT_UNKNOWN;
								if(mbase[i][0].isFunction()) {
									MathStructure mfunc(mbase[i][0]);
									mfunc.replace(mx, v);
									mfunc.eval(eo2);
									if(mfunc.isNumber()) cr = mbase[i][1].number().compare(mfunc.number());
								} else {
									cr = mbase[i][1].number().compare(mi.number());
								}
								b = COMPARISON_IS_NOT_EQUAL(cr);
								if(!b && cr != COMPARISON_RESULT_UNKNOWN) {v->destroy(); return false;}
							}
						}
					}
					if(!b) {
						CALCULATOR->endTemporaryStopMessages();
						CALCULATOR->endTemporaryStopMessages();
						CALCULATOR->error(false, _("To avoid division by zero, the following must be true: %s."), format_and_print(mbase).c_str(), NULL);
						CALCULATOR->beginTemporaryStopMessages();
						CALCULATOR->beginTemporaryStopMessages();
					}
				}
			}
			v->destroy();
		}
	} else if(m.isVariable()) {
		if(m.variable()->isKnown() && !check_denominators(((KnownVariable*) m.variable())->get(), mi, mx, eo)) return false;
	} else if(m.isFunction() && (m.function() == CALCULATOR->f_tan || m.function() == CALCULATOR->f_tanh || !m.representsNumber(true))) {
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		eo2.assume_denominators_nonzero = false;
		MathStructure mfunc(m);
		bool b = mfunc.calculateFunctions(eo2);
		if(b && !check_denominators(mfunc, mi, mx, eo)) return false;
		if(mfunc.function() == CALCULATOR->f_tan || mfunc.function() == CALCULATOR->f_tanh) {
			mfunc.replace(mx, mi);
			eo2.interval_calculation = INTERVAL_CALCULATION_INTERVAL_ARITHMETIC;
			CALCULATOR->beginTemporaryStopMessages();
			b = mfunc.calculateFunctions(eo2);
			CALCULATOR->endTemporaryStopMessages();
			if(!b) return false;
		}
	}
	return true;
}
bool replace_atanh(MathStructure &m, const MathStructure &x_var, const MathStructure &m1, const MathStructure &m2, const EvaluationOptions &eo) {
	bool b = false;
	if(m.isFunction() && m.function() == CALCULATOR->f_atanh && m.size() == 1 && m[0].contains(x_var, true)) {
		/*MathStructure mtest(m[0]);
		mtest.replace(x_var, m1);
		b = (mtest.compare(m_one) == COMPARISON_RESULT_EQUAL) || (mtest.compare(m_minus_one) == COMPARISON_RESULT_EQUAL);
		if(!b) {
			mtest = m[0];
			mtest.replace(x_var, m2);
			b = (mtest.compare(m_one) == COMPARISON_RESULT_EQUAL) || (mtest.compare(m_minus_one) == COMPARISON_RESULT_EQUAL);
		}*/
		b = true;
		if(b) {
			MathStructure marg(m[0]);
			m = marg;
			m += m_one;
			m.transform(CALCULATOR->f_ln);
			m *= nr_half;
			m += marg;
			m.last().negate();
			m.last() += m_one;
			m.last().transform(CALCULATOR->f_ln);
			m.last() *= Number(-1, 2);
			return true;
		}
	}
	for(size_t i = 0; i < m.size(); i++) {
		if(replace_atanh(m[i], x_var, m1, m2, eo)) b = true;
	}
	if(b) {
		m.childrenUpdated();
		m.calculatesub(eo, eo, false);
	}
	return b;
}
MathStructure *find_abs_x(MathStructure &mstruct, const MathStructure &x_var) {
	if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_abs && mstruct.size() == 1) {
		return &mstruct;
	}
	for(size_t i = 0; i < mstruct.size(); i++) {
		MathStructure *m = find_abs_x(mstruct[i], x_var);
		if(m) return m;
	}
	return NULL;
}
bool replace_abs(MathStructure &mstruct, const MathStructure &mabs, bool neg) {
	if(mstruct.equals(mabs, true, true)) {
		mstruct.setToChild(1, true);
		if(neg) mstruct.negate();
		return true;
	}
	bool b_ret = false;
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(replace_abs(mstruct[i], mabs, neg)) b_ret = true;
	}
	return b_ret;
}
bool contains_incalc_function(const MathStructure &mstruct, const EvaluationOptions &eo) {
	for(size_t i = 0; i < mstruct.size(); i++) {
		if(contains_incalc_function(mstruct[i], eo)) return true;
	}
	if(mstruct.isFunction()) {
		if((mstruct.function() == CALCULATOR->f_erf || mstruct.function() == CALCULATOR->f_Ei) && mstruct.size() == 1 && !mstruct[0].representsNonComplex()) {
			if(mstruct[0].representsComplex()) return true;
			MathStructure mtest(mstruct[0]);
			mtest.eval(eo);
			return !mtest.representsNonComplex();
		} else if((mstruct.function() == CALCULATOR->f_Ci || mstruct.function() == CALCULATOR->f_Si) && mstruct.size() == 1 && !mstruct[0].representsNonComplex()) {
			MathStructure mtest(mstruct[0]);
			mtest.eval(eo);
			return mtest.isNumber() && mtest.number().hasImaginaryPart() && mtest.number().hasRealPart();
		} else if(mstruct.function() == CALCULATOR->f_Li) {
			MathStructure mtest(mstruct);
			return !mtest.calculateFunctions(eo);
		} else if(mstruct.function() == CALCULATOR->f_igamma && mstruct.size() == 2) {
#if MPFR_VERSION_MAJOR < 4
			return true;
#else
			return !COMPARISON_IS_EQUAL_OR_LESS(mstruct[1].compare(m_zero));
#endif
		}
	}
	return false;
}

bool test_definite_ln(const MathStructure &m, const MathStructure &mi, const MathStructure &mx, const EvaluationOptions &eo) {
	for(size_t i = 0; i < m.size(); i++) {
		if(!test_definite_ln(m[i], mi, mx, eo)) return false;
	}
	if(m.function() == CALCULATOR->f_ln && m.size() == 1 && m[0].contains(mx, true) > 0 && !m[0].representsNonComplex(true)) {
		MathStructure mtest(m[0]);
		mtest.replace(mx, mi);
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		CALCULATOR->beginTemporaryStopMessages();
		mtest.eval(eo2);
		CALCULATOR->endTemporaryStopMessages();
		if(mtest.isNumber() && mtest.number().hasImaginaryPart() && !mtest.number().imaginaryPartIsNonZero() && !mtest.number().realPart().isNonNegative()) return false;
	}
	return true;
}

extern bool do_simplification(MathStructure &mstruct, const EvaluationOptions &eo, bool combine_divisions = true, bool only_gcd = false, bool combine_only = false, bool recursive = true, bool limit_size = false, int i_run = 1);

int IntegrateFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	MathStructure m1, m2;
	if(vargs.size() >= 2) m1 = vargs[1];
	else m1.setUndefined();
	if(vargs.size() >= 3) m2 = vargs[2];
	else m2.setUndefined();
	MathStructure x_var = vargs[3];
	if(m2.isUndefined() && (m1.isSymbolic() || (m1.isVariable() && !m1.variable()->isKnown()))) {x_var = m1; m1.setUndefined();}
	if(m1.isUndefined() != m2.isUndefined()) {
		if(m1.isUndefined()) m1.set(nr_minus_inf);
		else m2.set(nr_plus_inf);
	}
	m1.eval(eo);
	m2.eval(eo);
	int definite_integral = 0;
	if(!m1.isUndefined() && !m2.isUndefined()) definite_integral = -1;
	if(definite_integral < 0 && (!m1.isNumber() || !m1.number().isMinusInfinity()) && (!m2.isNumber() || !m2.number().isPlusInfinity())) definite_integral = 1;
	if(definite_integral > 0 && m1 == m2) {
		mstruct.clear();
		return 1;
	}
	CALCULATOR->beginTemporaryStopMessages();
	EvaluationOptions eo2 = eo;
	if(eo.approximation == APPROXIMATION_TRY_EXACT) eo2.approximation = APPROXIMATION_EXACT;
	CALCULATOR->beginTemporaryStopMessages();
	MathStructure mstruct_pre = vargs[0];
	MathStructure m_interval;
	if(!m1.isUndefined()) {
		m_interval.set(CALCULATOR->f_interval, &m1, &m2, NULL);
		CALCULATOR->beginTemporaryStopMessages();
		EvaluationOptions eo3 = eo;
		eo3.approximation = APPROXIMATION_APPROXIMATE;
		m_interval.calculateFunctions(eo3);
		CALCULATOR->endTemporaryStopMessages();
		UnknownVariable *var = new UnknownVariable("", format_and_print(vargs[3]));
		var->setInterval(m_interval);
		x_var.set(var);
		mstruct_pre.replace(vargs[3], x_var);
		var->destroy();
		if(definite_integral && !check_denominators(mstruct_pre, m_interval, x_var, eo)) {
			if(definite_integral < 0) {
				definite_integral = 0;
			} else {
				CALCULATOR->endTemporaryStopMessages();
				CALCULATOR->endTemporaryStopMessages(true);
				CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
				return false;
			}
		}
	}
	if(definite_integral) eo2.interval_calculation = INTERVAL_CALCULATION_INTERVAL_ARITHMETIC;
	mstruct = mstruct_pre;
	eo2.do_polynomial_division = false;
	mstruct.eval(eo2);
	if(definite_integral > 0 && mstruct.isAddition() && m1.isNumber() && m1.number().isReal() && m2.isNumber() && m2.number().isReal()) {
		mstruct.replace(x_var, vargs[3]);
		Number nr;
		for(size_t i = 0; i < mstruct.size();) {
			mstruct[i].transform(this);
			for(size_t i2 = 1; i2 < vargs.size(); i2++) mstruct[i].addChild(vargs[i2]);
			if(!mstruct[i].calculateFunctions(eo, false)) {
				CALCULATOR->endTemporaryStopMessages();
				CALCULATOR->endTemporaryStopMessages();
				CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
				return false;
			}
			if(mstruct[i].isNumber()) {
				if(nr.add(mstruct[i].number())) mstruct.delChild(i + 1);
				else i++;
			} else i++;
		}
		mstruct.childrenUpdated();
		if(mstruct.size() == 0) mstruct.set(nr, true);
		else if(!nr.isZero() || nr.isApproximate()) mstruct.addChild(nr);
		CALCULATOR->endTemporaryStopMessages(true);
		CALCULATOR->endTemporaryStopMessages(true);
		return 1;
	}
	do_simplification(mstruct, eo2, true, false, false, true, true);
	eo2.do_polynomial_division = eo.do_polynomial_division;
	MathStructure mbak(mstruct);

	if(vargs.size() < 5 || !vargs[4].number().getBoolean() || definite_integral == 0) {

		int use_abs = -1;
		/*if(m1.isUndefined() && x_var.representsReal() && !contains_complex(mstruct)) {
			use_abs = 1;
		}*/
		if(definite_integral) replace_atanh(mstruct, x_var, vargs[1], vargs[2], eo2);
		int b = mstruct.integrate(x_var, eo2, true, use_abs, definite_integral, true, (definite_integral && eo.approximation != APPROXIMATION_EXACT && PRECISION < 20) ? 2 : 4);
		if(b < 0) {
			mstruct = mbak;
			if(definite_integral) mstruct.replace(x_var, vargs[3]);
			CALCULATOR->endTemporaryStopMessages(true);
			CALCULATOR->endTemporaryStopMessages(true);
			CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
			return -1;
		}
		if((eo.approximation == APPROXIMATION_TRY_EXACT || (eo.approximation == APPROXIMATION_APPROXIMATE && mstruct.isApproximate())) && (!b || mstruct.containsFunction(this, true) > 0)) {
			vector<CalculatorMessage> blocked_messages;
			CALCULATOR->endTemporaryStopMessages(false, &blocked_messages);
			CALCULATOR->beginTemporaryStopMessages();
			MathStructure mbak_integ(mstruct);
			if(eo.approximation == APPROXIMATION_APPROXIMATE) eo2.approximation = APPROXIMATION_EXACT;
			else eo2.approximation = eo.approximation;
			eo2.do_polynomial_division = false;
			mstruct = mstruct_pre;
			if(definite_integral) replace_atanh(mstruct, x_var, vargs[1], vargs[2], eo2);
			mstruct.eval(eo2);
			do_simplification(mstruct, eo2, true, false, false, true, true);
			eo2.do_polynomial_division = eo.do_polynomial_division;
			int b2 = mstruct.integrate(x_var, eo2, true, use_abs, definite_integral, true, (definite_integral && eo.approximation != APPROXIMATION_EXACT && PRECISION < 20) ? 2 : 4);
			if(b2 < 0) {
				mstruct = mbak;
				if(definite_integral) mstruct.replace(x_var, vargs[3]);
				CALCULATOR->endTemporaryStopMessages(true);
				CALCULATOR->endTemporaryStopMessages(true);
				CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
				return -1;
			}
			if(b2 && (!b || mstruct.containsFunction(this, true) <= 0)) {
				CALCULATOR->endTemporaryStopMessages(true);
				b = true;
			} else {
				CALCULATOR->endTemporaryStopMessages(false);
				if(b) {
					CALCULATOR->addMessages(&blocked_messages);
					mstruct = mbak_integ;
				}
			}
		} else {
			CALCULATOR->endTemporaryStopMessages(true);
		}
		eo2.approximation = eo.approximation;
		if(b) {
			if(definite_integral && mstruct.containsFunction(this, true) <= 0 && test_definite_ln(mstruct, m_interval, x_var, eo)) {
				CALCULATOR->endTemporaryStopMessages(true);
				MathStructure mstruct_lower(mstruct);
				if(m1.isInfinite() || m2.isInfinite()) {
					CALCULATOR->beginTemporaryStopMessages();
					EvaluationOptions eo3 = eo;
					eo3.approximation = APPROXIMATION_EXACT;
					if(m1.isInfinite()) {
						b = mstruct_lower.calculateLimit(x_var, m1, eo3) && !mstruct_lower.isInfinite();
					} else {
						mstruct_lower.replace(x_var, vargs[1]);
						b = eo.approximation == APPROXIMATION_EXACT || !contains_incalc_function(mstruct_lower, eo);
					}
					MathStructure mstruct_upper(mstruct);
					if(m2.isInfinite()) {
						b = mstruct_upper.calculateLimit(x_var, m2, eo3) && !mstruct_upper.isInfinite();
					} else {
						mstruct_upper.replace(x_var, vargs[2]);
						b = eo.approximation == APPROXIMATION_EXACT || !contains_incalc_function(mstruct_upper, eo);
					}
					if(b) {
						mstruct = mstruct_upper;
						mstruct -= mstruct_lower;
						return 1;
					} else {
						if(definite_integral > 0) {
							CALCULATOR->endTemporaryStopMessages(true);
							mstruct = mbak;
							mstruct.replace(x_var, vargs[3]);
							CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
							return -1;
						} else {
							definite_integral = 0;
						}
					}
				} else {
					mstruct_lower.replace(x_var, vargs[1]);
					if(eo.approximation == APPROXIMATION_EXACT || !contains_incalc_function(mstruct_lower, eo)) {
						mstruct.replace(x_var, vargs[2]);
						if(eo.approximation != APPROXIMATION_EXACT && contains_incalc_function(mstruct, eo)) {
							mstruct = mbak;
						} else {
							mstruct -= mstruct_lower;
							return 1;
						}
					}
				}
			} else if(definite_integral < 0) {
				definite_integral = 0;
			} else if(definite_integral > 0) {
				mstruct = mbak;
			}
			if(!definite_integral) {
				if(!m1.isUndefined()) mstruct.replace(x_var, vargs[3]);
				CALCULATOR->endTemporaryStopMessages(true);
				mstruct += CALCULATOR->v_C;
				return 1;
			}
		}
		if(!b) {
			mstruct = mbak;
			if(definite_integral <= 0) {
				if(!m1.isUndefined()) mstruct.replace(x_var, vargs[3]);
				CALCULATOR->endTemporaryStopMessages(true);
				CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
				return -1;
			}
		}
		MathStructure *mabs = find_abs_x(mbak, x_var);
		if(mabs && !(*mabs)[0].representsNonComplex(true)) {
			MathStructure mtest((*mabs)[0]);
			mtest.transform(CALCULATOR->f_im);
			if(mtest.compare(m_zero) != COMPARISON_RESULT_EQUAL) mabs = NULL;
		}
		if(mabs) {
			bool b_reversed = COMPARISON_IS_EQUAL_OR_GREATER(m2.compare(m1));
			MathStructure m0((*mabs)[0]);
			m0.transform(COMPARISON_EQUALS, m_zero);
			EvaluationOptions eo3 = eo;
			eo3.approximation = APPROXIMATION_EXACT;
			eo3.isolate_x = true;
			eo3.isolate_var = &x_var;
			m0.eval(eo3);
			bool b_exit = true;
			if(m0.isZero()) {
				m0 = (*mabs)[0];
				m0.replace(x_var, vargs[1]);
				ComparisonResult cr1 = m0.compare(m_zero);
				if(COMPARISON_IS_EQUAL_OR_LESS(cr1)) {
					if(replace_abs(mbak, *mabs, false)) {
						mbak.replace(x_var, vargs[3]);
						mstruct.set(this, &mbak, &vargs[1], &vargs[2], &vargs[3], &m_zero, NULL);
						CALCULATOR->endTemporaryStopMessages(true);
						return 1;
					}
				} else if(COMPARISON_IS_EQUAL_OR_GREATER(cr1)) {
					if(replace_abs(mbak, *mabs, true)) {
						mbak.replace(x_var, vargs[3]);
						mstruct.set(this, &mbak, &vargs[1], &vargs[2], &vargs[3], &m_zero, NULL);
						CALCULATOR->endTemporaryStopMessages(true);
						return 1;
					}
				}
			} else if(m0.isComparison() && m0.comparisonType() == COMPARISON_EQUALS && m0[0] == x_var && m0[1].contains(x_var, true) == 0) {
				CALCULATOR->endTemporaryStopMessages();
				CALCULATOR->beginTemporaryStopMessages();
				m0.setToChild(2, true);
				ComparisonResult cr1 = m0.compare(b_reversed ? m2 : m1);
				ComparisonResult cr2 = m0.compare(b_reversed ? m1 : m2);
				if(COMPARISON_IS_EQUAL_OR_GREATER(cr1) || COMPARISON_IS_EQUAL_OR_LESS(cr2)) {
					MathStructure mtest((*mabs)[0]);
					if(b_reversed) mtest.replace(x_var, COMPARISON_IS_EQUAL_OR_GREATER(cr1) ? vargs[1] : vargs[2]);
					else mtest.replace(x_var, COMPARISON_IS_EQUAL_OR_GREATER(cr1) ? vargs[2] : vargs[1]);
					ComparisonResult cr = mtest.compare(m_zero);
					if(COMPARISON_IS_EQUAL_OR_LESS(cr)) {
						if(replace_abs(mbak, *mabs, false)) {
							mbak.replace(x_var, vargs[3]);
							mstruct.set(this, &mbak, &vargs[1], &vargs[2], &vargs[3], &m_zero, NULL);
							CALCULATOR->endTemporaryStopMessages(true);
							return 1;
						}
					} else if(COMPARISON_IS_EQUAL_OR_GREATER(cr)) {
						if(replace_abs(mbak, *mabs, true)) {
							mbak.replace(x_var, vargs[3]);
							mstruct.set(this, &mbak, &vargs[1], &vargs[2], &vargs[3], &m_zero, NULL);
							CALCULATOR->endTemporaryStopMessages(true);
							return 1;
						}
					}
				} else if(cr1 == COMPARISON_RESULT_LESS && cr2 == COMPARISON_RESULT_GREATER) {
					MathStructure mtest((*mabs)[0]);
					mtest.replace(x_var, vargs[1]);
					ComparisonResult cr = mtest.compare(m_zero);
					MathStructure m1(mbak), m2, minteg1, minteg2;
					b = false;
					if(COMPARISON_IS_EQUAL_OR_LESS(cr)) {
						if(replace_abs(m1, *mabs, false)) b = true;
					} else if(COMPARISON_IS_EQUAL_OR_GREATER(cr)) {
						if(replace_abs(m1, *mabs, true)) b = true;
					}
					if(b) {
						m1.replace(x_var, vargs[3]);
						minteg1.set(this, &m1, &vargs[1], &m0, &vargs[3], &m_zero, NULL);
						CALCULATOR->beginTemporaryStopMessages();
						minteg1.eval(eo);
						b_exit = CALCULATOR->endTemporaryStopMessages(NULL, NULL, MESSAGE_ERROR) == 0;
						b = b_exit && minteg1.containsFunction(this, true) <= 0;
					}
					if(b) {
						mtest = (*mabs)[0];
						mtest.replace(x_var, vargs[2]);
						cr = mtest.compare(m_zero);
						m2 = mbak;
						b = false;
						if(COMPARISON_IS_EQUAL_OR_LESS(cr)) {
							if(replace_abs(m2, *mabs, false)) b = true;
						} else if(COMPARISON_IS_EQUAL_OR_GREATER(cr)) {
							if(replace_abs(m2, *mabs, true)) b = true;
						}
					}
					if(b) {
						m2.replace(x_var, vargs[3]);
						minteg2.set(this, &m2, &m0, &vargs[2], &vargs[3], &m_zero, NULL);
						CALCULATOR->beginTemporaryStopMessages();
						minteg2.eval(eo);
						b_exit = CALCULATOR->endTemporaryStopMessages(NULL, NULL, MESSAGE_ERROR) == 0;
						b = b_exit && minteg2.containsFunction(this, true) <= 0;
					}
					if(b) {
						CALCULATOR->endTemporaryStopMessages(true);
						mstruct = minteg1;
						mstruct += minteg2;
						return 1;
					}
				}
				if(b_exit) {
					CALCULATOR->endTemporaryStopMessages();
					mstruct = mbak;
					mstruct.replace(x_var, vargs[3]);
					CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
					return -1;
				}
			}
		}
		CALCULATOR->endTemporaryStopMessages();
		if(mstruct.containsInterval() && eo.approximation == APPROXIMATION_EXACT) {
			CALCULATOR->error(false, _("Unable to integrate the expression exact."), NULL);
			mstruct.replace(x_var, vargs[3]);
			return -1;
		}
	} else {
		CALCULATOR->endTemporaryStopMessages();
		CALCULATOR->endTemporaryStopMessages();
	}
	if(m1.isNumber() && m1.number().isReal() && m2.isNumber() && m2.number().isReal()) {

		if(eo.approximation != APPROXIMATION_EXACT) eo2.approximation = APPROXIMATION_APPROXIMATE;

		mstruct = mstruct_pre;
		mstruct.eval(eo2);

		eo2.interval_calculation = INTERVAL_CALCULATION_SIMPLE_INTERVAL_ARITHMETIC;
		eo2.warn_about_denominators_assumed_nonzero = false;

		Number nr_begin, nr_end;
		bool b_reversed = false;
		if(m1.number().isGreaterThan(m2.number())) {
			nr_begin = m2.number();
			nr_end = m1.number();
			b_reversed = true;
		} else {
			nr_begin = m1.number();
			nr_end = m2.number();
		}
		if(eo.approximation != APPROXIMATION_EXACT) {
			Number nr;
			CALCULATOR->beginTemporaryStopMessages();
			if(romberg(mstruct, nr, x_var, eo2, nr_begin, nr_end)) {
				CALCULATOR->endTemporaryStopMessages();
				if(b_reversed) nr.negate();
				mstruct = nr;
				if(vargs.size() < 5 || !vargs[4].number().getBoolean()) CALCULATOR->error(false, _("Definite integral was approximated."), NULL);
				return 1;
			}
			CALCULATOR->endTemporaryStopMessages();
			mstruct = mbak;
			mstruct.replace(x_var, vargs[3]);
			CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
			return -1;
		}
		Number nr_range(nr_end);
		nr_range -= nr_begin;
		MathStructure merr(mstruct);
		CALCULATOR->beginTemporaryStopMessages();
		eo2.expand = false;
		for(size_t i = 0; i < 4; i++) {
			if(merr.containsFunction(CALCULATOR->f_diff, true) > 0 || !merr.differentiate(x_var, eo2)) {
				break;
			}
			merr.calculatesub(eo2, eo2, true);
			if(CALCULATOR->aborted() || merr.countTotalChildren() > 200) {
				break;
			}
		}
		eo2.expand = eo.expand;
		CALCULATOR->endTemporaryStopMessages();
		if(merr.isZero()) {
			Number nr;
			if(numerical_integration(mstruct, nr, x_var, eo2, nr_begin, nr_end, 12, 0)) {
				if(b_reversed) nr.negate();
				mstruct = nr;
				return 1;
			}
		}
		CALCULATOR->error(false, _("Unable to integrate the expression exact."), NULL);
		mstruct = mbak;
		mstruct.replace(x_var, vargs[3]);
		return -1;
	}
	mstruct = mbak;
	mstruct.replace(x_var, vargs[3]);
	CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
	return -1;
}

RombergFunction::RombergFunction() : MathFunction("romberg", 3, 6) {
	Argument *arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
	NON_COMPLEX_NUMBER_ARGUMENT(2)
	NON_COMPLEX_NUMBER_ARGUMENT(3)
	setCondition("\\z > \\y");
	IntegerArgument *iarg = new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG);
	Number nr(2, 1);
	iarg->setMin(&nr);
	setArgumentDefinition(4, iarg);
	setDefaultValue(4, "6");
	setArgumentDefinition(5, new IntegerArgument("", ARGUMENT_MIN_MAX_NONE, true, true, INTEGER_TYPE_SLONG));
	setDefaultValue(5, "20");
	setArgumentDefinition(6, new SymbolicArgument());
	setDefaultValue(6, "undefined");
}
int RombergFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	MathStructure minteg(vargs[0]);
	EvaluationOptions eo2 = eo;
	eo2.approximation = APPROXIMATION_APPROXIMATE;
	Number nr_interval;
	nr_interval.setInterval(vargs[1].number(), vargs[2].number());
	UnknownVariable *var = new UnknownVariable("", format_and_print(vargs[5]));
	var->setInterval(nr_interval);
	MathStructure x_var(var);
	minteg.replace(vargs[5], x_var);
	var->destroy();
	minteg.eval(eo2);
	Number nr;
	eo2.interval_calculation = INTERVAL_CALCULATION_SIMPLE_INTERVAL_ARITHMETIC;
	eo2.warn_about_denominators_assumed_nonzero = false;
	CALCULATOR->beginTemporaryStopMessages();
	if(romberg(minteg, nr, x_var, eo2, vargs[1].number(), vargs[2].number(), vargs[4].number().lintValue(), vargs[3].number().lintValue(), false)) {
		CALCULATOR->endTemporaryStopMessages();
		mstruct = nr;
		return 1;
	}
	CALCULATOR->endTemporaryStopMessages();
	CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
	return 0;
}
MonteCarloFunction::MonteCarloFunction() : MathFunction("montecarlo", 4, 5) {
	Argument *arg = new Argument("", false, false);
	arg->setHandleVector(true);
	setArgumentDefinition(1, arg);
	NON_COMPLEX_NUMBER_ARGUMENT(2)
	NON_COMPLEX_NUMBER_ARGUMENT(3)
	setCondition("\\z > \\y");
	setArgumentDefinition(4, new IntegerArgument("", ARGUMENT_MIN_MAX_POSITIVE));
	setArgumentDefinition(5, new SymbolicArgument());
	setDefaultValue(5, "undefined");
}
int MonteCarloFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	MathStructure minteg(vargs[0]);
	EvaluationOptions eo2 = eo;
	eo2.approximation = APPROXIMATION_APPROXIMATE;
	Number nr_interval;
	nr_interval.setInterval(vargs[1].number(), vargs[2].number());
	UnknownVariable *var = new UnknownVariable("", format_and_print(vargs[4]));
	var->setInterval(nr_interval);
	MathStructure x_var(var);
	minteg.replace(vargs[4], x_var);
	var->destroy();
	minteg.eval(eo2);
	Number nr;
	eo2.interval_calculation = INTERVAL_CALCULATION_NONE;
	if(montecarlo(minteg, nr, x_var, eo2, vargs[1].number(), vargs[2].number(), vargs[3].number())) {
		mstruct = nr;
		return 1;
	}
	CALCULATOR->error(false, _("Unable to integrate the expression."), NULL);
	return 0;
}

SolveFunction::SolveFunction() : MathFunction("solve", 1, 2) {
	setArgumentDefinition(2, new SymbolicArgument());
	setDefaultValue(2, "undefined");
}
bool is_comparison_structure(const MathStructure &mstruct, const MathStructure &xvar, bool *bce = NULL, bool do_bce_or = false);
bool is_comparison_structure(const MathStructure &mstruct, const MathStructure &xvar, bool *bce, bool do_bce_or) {
	if(mstruct.isComparison()) {
		if(bce) *bce = mstruct.comparisonType() == COMPARISON_EQUALS && mstruct[0] == xvar;
		return true;
	}
	if(bce && do_bce_or && mstruct.isLogicalOr()) {
		*bce = true;
		for(size_t i = 0; i < mstruct.size(); i++) {
			bool bcei = false;
			if(!is_comparison_structure(mstruct[i], xvar, &bcei, false)) return false;
			if(!bcei) *bce = false;
		}
		return true;
	}
	if(bce) *bce = false;
	if(mstruct.isLogicalAnd()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(is_comparison_structure(mstruct[i], xvar)) return true;
		}
		return true;
	} else if(mstruct.isLogicalOr()) {
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(!is_comparison_structure(mstruct[i], xvar)) return false;
		}
		return true;
	}
	return false;
}

MathStructure *solve_handle_logical_and(MathStructure &mstruct, MathStructure **mtruefor, ComparisonType ct, bool &b_partial, const MathStructure &x_var) {
	MathStructure *mcondition = NULL;
	for(size_t i2 = 0; i2 < mstruct.size(); ) {
		if(ct == COMPARISON_EQUALS) {
			if(mstruct[i2].isComparison() && ct == mstruct[i2].comparisonType() && mstruct[i2][0].contains(x_var)) {
				if(mstruct[i2][0] == x_var) {
					if(mstruct.size() == 2) {
						if(i2 == 0) {
							mstruct[1].ref();
							mcondition = &mstruct[1];
						} else {
							mstruct[0].ref();
							mcondition = &mstruct[0];
						}
					} else {
						mcondition = new MathStructure();
						mcondition->set_nocopy(mstruct);
						mcondition->delChild(i2 + 1);
					}
					mstruct.setToChild(i2 + 1, true);
					break;
				} else {
					b_partial = true;
					i2++;
				}
			} else {
				i2++;
			}
		} else {
			if(mstruct[i2].isComparison() && mstruct[i2][0].contains(x_var)) {
				i2++;
			} else {
				mstruct[i2].ref();
				if(mcondition) {
					mcondition->add_nocopy(&mstruct[i2], OPERATION_LOGICAL_AND, true);
				} else {
					mcondition = &mstruct[i2];
				}
				mstruct.delChild(i2 + 1);
			}
		}
	}
	if(ct == COMPARISON_EQUALS) {
		if(mstruct.isLogicalAnd()) {
			MathStructure *mtmp = new MathStructure();
			mtmp->set_nocopy(mstruct);
			if(!(*mtruefor)) {
				*mtruefor = mtmp;
			} else {
				(*mtruefor)->add_nocopy(mtmp, OPERATION_LOGICAL_OR, true);
			}
			mstruct.clear(true);
		}
	} else {
		if(mstruct.size() == 1) {
			mstruct.setToChild(1, true);
			if(ct != COMPARISON_EQUALS) mstruct.setProtected();
		} else if(mstruct.size() == 0) {
			mstruct.clear(true);
			if(!(*mtruefor)) {
				*mtruefor = mcondition;
			} else {
				(*mtruefor)->add_nocopy(mcondition, OPERATION_LOGICAL_OR, true);
			}
			mcondition = NULL;
		} else if(ct != COMPARISON_EQUALS) {
			for(size_t i = 0; i < mstruct.size(); i++) {
				mstruct[i].setProtected();
			}
		}
	}
	return mcondition;
}

void simplify_constant(MathStructure &mstruct, const MathStructure &x_var, const MathStructure &y_var, const MathStructure &c_var, bool in_comparison = false, bool in_or = false, bool in_and = false);
void simplify_constant(MathStructure &mstruct, const MathStructure &x_var, const MathStructure &y_var, const MathStructure &c_var, bool in_comparison, bool in_or, bool in_and) {
	if(!in_comparison && mstruct.isComparison()) {
		if(mstruct[0] == c_var) {
			if(in_or) mstruct.clear(true);
			else mstruct.set(1, 1, 0);
		} else if(mstruct[0] == y_var) {
			if(mstruct[1].contains(y_var, true) <= 0) simplify_constant(mstruct[1], x_var, y_var, c_var, true);
		} else if(mstruct[0].contains(y_var, true) <= 0 && mstruct.contains(c_var, true) > 0) {
			if(in_or) mstruct.clear(true);
			else mstruct.set(1, 1, 0);
		}
	}
	if(in_comparison) {
		if(mstruct.contains(y_var, true) <= 0 && mstruct.contains(x_var, true) <= 0 && mstruct.contains(c_var, true) > 0) {
			mstruct = c_var;
			return;
		}
	}
	if(in_comparison) {
		int n_c = 0, b_cx = false;
		size_t i_c = 0;
		for(size_t i = 0; i < mstruct.size(); i++) {
			if(mstruct[i].contains(c_var, true) > 0) {
				n_c++;
				i_c = i;
				if(!b_cx && mstruct[i].contains(x_var, true) > 0) {
					b_cx = true;
				}
			}
		}
		if(!b_cx && n_c >= 1 && (mstruct.isAddition() || mstruct.isMultiplication())) {
			bool b_c = false;
			for(size_t i = 0; i < mstruct.size();) {
				if(mstruct[i].contains(c_var, true) > 0) {
					if(b_c) {
						mstruct.delChild(i + 1);
					} else {
						b_c = true;
						mstruct[i] = c_var;
						i++;
					}
				} else if(mstruct[i].contains(x_var, true) <= 0) {
					mstruct.delChild(i + 1);
				} else {
					i++;
				}
			}
			if(mstruct.size() == 1) mstruct.setToChild(1, true);
		} else if(n_c == 1) {
			if(b_cx) simplify_constant(mstruct[i_c], x_var, y_var, c_var, true);
			else mstruct[i_c] = c_var;
		}
	} else {
		for(size_t i = 0; i < mstruct.size(); i++) {
			simplify_constant(mstruct[i], x_var, y_var, c_var, false, mstruct.isLogicalOr(), mstruct.isLogicalAnd());
		}
	}
}

extern int test_comparisons(const MathStructure &msave, MathStructure &mthis, const MathStructure &x_var, const EvaluationOptions &eo, bool sub = false);
int test_equation(MathStructure &mstruct, const EvaluationOptions &eo, const MathStructure &x_var, const MathStructure &y_var, const MathStructure &x_value, const MathStructure &y_value) {
	if(mstruct.isComparison() && mstruct.comparisonType() == COMPARISON_EQUALS && mstruct[0] == y_var) {
		MathStructure mtest(mstruct);
		mtest.replace(x_var, x_value);
		MathStructure mtest2(y_var);
		mtest2.transform(COMPARISON_EQUALS, y_value);
		CALCULATOR->beginTemporaryStopMessages();
		EvaluationOptions eo2 = eo;
		eo2.approximation = APPROXIMATION_APPROXIMATE;
		mtest.calculateFunctions(eo2);
		mtest2.calculateFunctions(eo2);
		int b = test_comparisons(mtest, mtest2, y_var, eo);
		CALCULATOR->endTemporaryStopMessages();
		if(!b) mstruct.clear(true);
		return b;
	}
	bool b_ret = 0;
	for(size_t i = 0; i < mstruct.size(); i++) {
		int b = test_equation(mstruct[i], eo, x_var, y_var, x_value, y_value);
		if(b < 0) return b;
		else if(b > 0) b_ret = 1;
	}
	return b_ret;
}

int solve_equation(MathStructure &mstruct, const MathStructure &m_eqn, const MathStructure &y_var, const EvaluationOptions &eo, bool dsolve = false, const MathStructure &x_var = m_undefined, const MathStructure &c_var = m_undefined, const MathStructure &x_value = m_undefined, const MathStructure &y_value = m_undefined) {

	int itry = 0;
	int ierror = 0;
	int first_error = 0;

	Assumptions *assumptions = NULL;
	bool assumptions_added = false;
	AssumptionSign as = ASSUMPTION_SIGN_UNKNOWN;
	AssumptionType at = ASSUMPTION_TYPE_NUMBER;
	MathStructure msave;
	string strueforall;

	while(true) {

		if(itry == 1) {
			if(ierror == 1) {
				if(!dsolve) CALCULATOR->error(true, _("No equality or inequality to solve. The entered expression to solve is not correct (ex. \"x + 5 = 3\" is correct)"), NULL);
				return -1;
			} else {
				first_error = ierror;
				msave = mstruct;
			}
		}

		itry++;

		if(itry == 2) {
			if(y_var.isVariable() && y_var.variable()->subtype() == SUBTYPE_UNKNOWN_VARIABLE) {
				assumptions = ((UnknownVariable*) y_var.variable())->assumptions();
				if(!assumptions) {
					assumptions = new Assumptions();
					assumptions->setSign(CALCULATOR->defaultAssumptions()->sign());
					assumptions->setType(CALCULATOR->defaultAssumptions()->type());
					((UnknownVariable*) y_var.variable())->setAssumptions(assumptions);
					assumptions_added = true;
				}
			} else {
				assumptions = CALCULATOR->defaultAssumptions();
			}
			if(assumptions->sign() != ASSUMPTION_SIGN_UNKNOWN) {
				as = assumptions->sign();
				assumptions->setSign(ASSUMPTION_SIGN_UNKNOWN);
			} else {
				itry++;
			}
		}
		if(itry == 3) {
			if(assumptions->type() > ASSUMPTION_TYPE_NUMBER) {
				at = assumptions->type();
				assumptions->setType(ASSUMPTION_TYPE_NUMBER);
				as = assumptions->sign();
				assumptions->setSign(ASSUMPTION_SIGN_UNKNOWN);
			} else {
				itry++;
			}
		}

		if(itry > 3) {
			if(as != ASSUMPTION_SIGN_UNKNOWN) assumptions->setSign(as);
			if(at > ASSUMPTION_TYPE_NUMBER) assumptions->setType(at);
			if(assumptions_added) ((UnknownVariable*) y_var.variable())->setAssumptions(NULL);
			switch(first_error) {
				case 2: {
					CALCULATOR->error(true, _("The comparison is true for all %s (with current assumptions)."), format_and_print(y_var).c_str(), NULL);
					break;
				}
				case 3: {
					CALCULATOR->error(true, _("No possible solution was found (with current assumptions)."), NULL);
					break;
				}
				case 4: {
					CALCULATOR->error(true, _("Was unable to completely isolate %s."), format_and_print(y_var).c_str(), NULL);
					break;
				}
				case 7: {
					CALCULATOR->error(false, _("The comparison is true for all %s if %s."), format_and_print(y_var).c_str(), strueforall.c_str(), NULL);
					break;
				}
				default: {
					CALCULATOR->error(true, _("Was unable to isolate %s."), format_and_print(y_var).c_str(), NULL);
					break;
				}
			}
			mstruct = msave;
			return -1;
		}

		ComparisonType ct = COMPARISON_EQUALS;

		bool b = false;
		bool b_partial = false;

		if(m_eqn.isComparison()) {
			ct = m_eqn.comparisonType();
			mstruct = m_eqn;
			b = true;
		} else if(m_eqn.isLogicalAnd() && m_eqn.size() > 0 && m_eqn[0].isComparison()) {
			ct = m_eqn[0].comparisonType();
			for(size_t i = 0; i < m_eqn.size(); i++) {
				if(m_eqn[i].isComparison() && m_eqn[i].contains(y_var, true) > 0) {
					ct = m_eqn[i].comparisonType();
					break;
				}
			}
			mstruct = m_eqn;
			b = true;
		} else if(m_eqn.isLogicalOr() && m_eqn.size() > 0 && m_eqn[0].isComparison()) {
			ct = m_eqn[0].comparisonType();
			mstruct = m_eqn;
			b = true;
		} else if(m_eqn.isLogicalOr() && m_eqn.size() > 0 && m_eqn[0].isLogicalAnd() && m_eqn[0].size() > 0 && m_eqn[0][0].isComparison()) {
			ct = m_eqn[0][0].comparisonType();
			for(size_t i = 0; i < m_eqn[0].size(); i++) {
				if(m_eqn[0][i].isComparison() && m_eqn[0][i].contains(y_var, true) > 0) {
					ct = m_eqn[0][i].comparisonType();
					break;
				}
			}
			mstruct = m_eqn;
			b = true;
		} else if(m_eqn.isVariable() && m_eqn.variable()->isKnown() && (eo.approximation != APPROXIMATION_EXACT || !m_eqn.variable()->isApproximate()) && ((KnownVariable*) m_eqn.variable())->get().isComparison()) {
			mstruct = ((KnownVariable*) m_eqn.variable())->get();
			mstruct.unformat();
			ct = m_eqn.comparisonType();
			b = true;
		} else {
			EvaluationOptions eo2 = eo;
			eo2.test_comparisons = false;
			eo2.assume_denominators_nonzero = false;
			eo2.isolate_x = false;
			mstruct = m_eqn;
			mstruct.eval(eo2);
			if(mstruct.isComparison()) {
				ct = mstruct.comparisonType();
				b = true;
			} else if(mstruct.isLogicalAnd() && mstruct.size() > 0 && mstruct[0].isComparison()) {
				ct = mstruct[0].comparisonType();
				b = true;
			} else if(mstruct.isLogicalOr() && mstruct.size() > 0 && mstruct[0].isComparison()) {
				ct = mstruct[0].comparisonType();
				b = true;
			} else if(mstruct.isLogicalOr() && mstruct.size() > 0 && mstruct[0].isLogicalAnd() && mstruct[0].size() > 0 && mstruct[0][0].isComparison()) {
				ct = mstruct[0][0].comparisonType();
				b = true;
			}
		}

		if(!b) {
			ierror = 1;
			continue;
		}

		EvaluationOptions eo2 = eo;
		eo2.isolate_var = &y_var;
		eo2.isolate_x = true;
		eo2.test_comparisons = true;
		mstruct.eval(eo2);
		if(dsolve) {
			if(x_value.isUndefined() || y_value.isUndefined()) {
				simplify_constant(mstruct, x_var, y_var, c_var);
				mstruct.eval(eo2);
			} else {
				int test_r = test_equation(mstruct, eo2, x_var, y_var, x_value, y_value);
				if(test_r < 0) {
					ierror = 8;
					continue;
				} else if(test_r > 0) {
					mstruct.eval(eo2);
				}
			}
		}

		if(mstruct.isOne()) {
			ierror = 2;
			continue;
		} else if(mstruct.isZero()) {
			ierror = 3;
			continue;
		}

		if(mstruct.isComparison()) {
			if((ct == COMPARISON_EQUALS && mstruct.comparisonType() != COMPARISON_EQUALS) || !mstruct.contains(y_var)) {
				if(itry == 1) {
					strueforall = format_and_print(mstruct);
				}
				ierror = 7;
				continue;
			} else if(ct == COMPARISON_EQUALS && mstruct[0] != y_var) {
				ierror = 4;
				continue;
			}
			if(ct == COMPARISON_EQUALS) {
				mstruct.setToChild(2, true);
			} else {
				mstruct.setProtected();
			}
			if(itry > 1) {
				assumptions->setSign(as);
				if(itry == 2) {
					CALCULATOR->error(false, _("Was unable to isolate %s with the current assumptions. The assumed sign was therefore temporarily set as unknown."), format_and_print(y_var).c_str(), NULL);
				} else if(itry == 3) {
					assumptions->setType(at);
					CALCULATOR->error(false, _("Was unable to isolate %s with the current assumptions. The assumed type and sign was therefore temporarily set as unknown."), format_and_print(y_var).c_str(), NULL);
				}
				if(assumptions_added) ((UnknownVariable*) y_var.variable())->setAssumptions(NULL);
			}
			return 1;
		} else if(mstruct.isLogicalAnd()) {
			MathStructure *mtruefor = NULL;
			bool b_partial;
			MathStructure mcopy(mstruct);
			MathStructure *mcondition = solve_handle_logical_and(mstruct, &mtruefor, ct, b_partial, y_var);
			if((!mstruct.isComparison() && !mstruct.isLogicalAnd()) || (ct == COMPARISON_EQUALS && (!mstruct.isComparison() || mstruct.comparisonType() != COMPARISON_EQUALS || mstruct[0] != y_var)) || !mstruct.contains(y_var)) {
				if(mtruefor) delete mtruefor;
				if(mcondition) delete mcondition;
				if(b_partial) {
					ierror = 4;
				} else {
					ierror = 5;
				}
				mstruct = mcopy;
				continue;
			}
			if(itry > 1) {
				assumptions->setSign(as);
				if(itry == 2) {
					CALCULATOR->error(false, _("Was unable to isolate %s with the current assumptions. The assumed sign was therefore temporarily set as unknown."), format_and_print(y_var).c_str(), NULL);
				} else if(itry == 3) {
					assumptions->setType(at);
					CALCULATOR->error(false, _("Was unable to isolate %s with the current assumptions. The assumed type and sign was therefore temporarily set as unknown."), format_and_print(y_var).c_str(), NULL);
				}
				if(assumptions_added) ((UnknownVariable*) y_var.variable())->setAssumptions(NULL);
			}
			if(mcondition) {
				CALCULATOR->error(false, _("The solution requires that %s."), format_and_print(mcondition).c_str(), NULL);
				delete mcondition;
			}
			if(mtruefor) {
				CALCULATOR->error(false, _("The comparison is true for all %s if %s."), format_and_print(y_var).c_str(), format_and_print(mtruefor).c_str(), NULL);
				delete mtruefor;
			}
			if(ct == COMPARISON_EQUALS) mstruct.setToChild(2, true);
			return 1;
		} else if(mstruct.isLogicalOr()) {
			MathStructure mcopy(mstruct);
			MathStructure *mtruefor = NULL;
			vector<MathStructure*> mconditions;
			for(size_t i = 0; i < mstruct.size(); ) {
				MathStructure *mcondition = NULL;
				bool b_and = false;
				if(mstruct[i].isLogicalAnd()) {
					mcondition = solve_handle_logical_and(mstruct[i], &mtruefor, ct, b_partial, y_var);
					b_and = true;
				}
				if(!mstruct[i].isZero()) {
					for(size_t i2 = 0; i2 < i; i2++) {
						if(mstruct[i2] == mstruct[i]) {
							mstruct[i].clear();
							if(mcondition && mconditions[i2]) {
								mconditions[i2]->add_nocopy(mcondition, OPERATION_LOGICAL_OR, true);
							}
							break;
						}
					}
				}
				bool b_del = false;
				if((!mstruct[i].isComparison() && !mstruct[i].isLogicalAnd()) || (ct == COMPARISON_EQUALS && (!mstruct[i].isComparison() || mstruct[i].comparisonType() != COMPARISON_EQUALS)) || !mstruct[i].contains(y_var)) {
					b_del = true;
				} else if(ct == COMPARISON_EQUALS && mstruct[i][0] != y_var) {
					b_partial = true;
					b_del = true;
				}
				if(b_del) {
					if(!mstruct[i].isZero()) {
						mstruct[i].ref();
						if(!mtruefor) {
							mtruefor = &mstruct[i];
						} else {
							mtruefor->add_nocopy(&mstruct[i], OPERATION_LOGICAL_OR, true);
						}
					}
					mstruct.delChild(i + 1);
				} else {
					mconditions.push_back(mcondition);
					if(!b_and && ct != COMPARISON_EQUALS) mstruct[i].setProtected();
					i++;
				}
			}
			if(ct == COMPARISON_EQUALS) {
				for(size_t i = 0; i < mstruct.size(); i++) {
					if(mstruct[i].isComparison() && mstruct[i].comparisonType() == ct) mstruct[i].setToChild(2, true);
				}
			}
			if(mstruct.size() == 1) {
				mstruct.setToChild(1, true);
			} else if(mstruct.size() == 0) {
				if(mtruefor) delete mtruefor;
				if(b_partial) ierror = 4;
				else ierror = 5;
				mstruct = mcopy;
				continue;
			} else {
				mstruct.setType(STRUCT_VECTOR);
			}
			if(itry > 1) {
				assumptions->setSign(as);
				if(itry == 2) {
					CALCULATOR->error(false, _("Was unable to isolate %s with the current assumptions. The assumed sign was therefore temporarily set as unknown."), format_and_print(y_var).c_str(), NULL);
				} else if(itry == 3) {
					assumptions->setType(at);
					CALCULATOR->error(false, _("Was unable to isolate %s with the current assumptions. The assumed type and sign was therefore temporarily set as unknown."), format_and_print(y_var).c_str(), NULL);
				}
				if(assumptions_added) ((UnknownVariable*) y_var.variable())->setAssumptions(NULL);
			}

			if(mconditions.size() == 1) {
				if(mconditions[0]) {
					CALCULATOR->error(false, _("The solution requires that %s."), format_and_print(mconditions[0]).c_str(), NULL);
					delete mconditions[0];
				}
			} else {
				string sconditions;
				for(size_t i = 0; i < mconditions.size(); i++) {
					if(mconditions[i]) {
						CALCULATOR->error(false, _("Solution %s requires that %s."), i2s(i + 1).c_str(), format_and_print(mconditions[i]).c_str(), NULL);
						delete mconditions[i];
					}
				}
			}
			if(mtruefor) {
				CALCULATOR->error(false, _("The comparison is true for all %s if %s."), format_and_print(y_var).c_str(), format_and_print(mtruefor).c_str(), NULL);
				delete mtruefor;
			}
			return 1;
		} else {
			ierror = 6;
		}
	}
	return -1;

}

int SolveFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	return solve_equation(mstruct, vargs[0], vargs[1], eo);
}

SolveMultipleFunction::SolveMultipleFunction() : MathFunction("multisolve", 2) {
	setArgumentDefinition(1, new VectorArgument());
	VectorArgument *arg = new VectorArgument();
	arg->addArgument(new SymbolicArgument());
	arg->setReoccuringArguments(true);
	setArgumentDefinition(2, arg);
	setCondition("dimension(\\x) = dimension(\\y)");
}
int SolveMultipleFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	mstruct.clearVector();

	if(vargs[1].size() < 1) return 1;

	vector<bool> eleft;
	eleft.resize(vargs[0].size(), true);
	vector<size_t> eorder;
	bool b = false;
	for(size_t i = 0; i < vargs[1].size(); i++) {
		b = false;
		for(size_t i2 = 0; i2 < vargs[0].size(); i2++) {
			if(eleft[i2] && vargs[0][i2].contains(vargs[1][i], true)) {
				eorder.push_back(i2);
				eleft[i2] = false;
				b = true;
				break;
			}
		}
		if(!b) {
			eorder.clear();
			for(size_t i2 = 0; i2 < vargs[0].size(); i2++) {
				eorder.push_back(i2);
			}
			break;
		}
	}

	for(size_t i = 0; i < eorder.size(); i++) {
		MathStructure msolve(vargs[0][eorder[i]]);
		EvaluationOptions eo2 = eo;
		eo2.isolate_var = &vargs[1][i];
		for(size_t i2 = 0; i2 < i; i2++) {
			msolve.replace(vargs[1][i2], mstruct[i2]);
		}
		msolve.eval(eo2);

		if(msolve.isComparison()) {
			if(msolve[0] != vargs[1][i]) {
				if(!b) {
					CALCULATOR->error(true, _("Unable to isolate %s.\n\nYou might need to place the equations and variables in an appropriate order so that each equation at least contains the corresponding variable (if automatic reordering failed)."), format_and_print(vargs[1][i]).c_str(), NULL);
				} else {
					CALCULATOR->error(true, _("Unable to isolate %s."), format_and_print(vargs[1][i]).c_str(), NULL);
				}
				return 0;
			} else {
				if(msolve.comparisonType() == COMPARISON_EQUALS) {
					mstruct.addChild(msolve[1]);
				} else {
					CALCULATOR->error(true, _("Inequalities is not allowed in %s()."), preferredName().name.c_str(), NULL);
					return 0;
				}
			}
		} else if(msolve.isLogicalOr()) {
			for(size_t i2 = 0; i2 < msolve.size(); i2++) {
				if(!msolve[i2].isComparison() || msolve[i2].comparisonType() != COMPARISON_EQUALS || msolve[i2][0] != vargs[1][i]) {
					CALCULATOR->error(true, _("Unable to isolate %s."), format_and_print(vargs[1][i]).c_str(), NULL);
					return 0;
				} else {
					msolve[i2].setToChild(2, true);
				}
			}
			msolve.setType(STRUCT_VECTOR);
			mstruct.addChild(msolve);
		} else {
			CALCULATOR->error(true, _("Unable to isolate %s."), format_and_print(vargs[1][i]).c_str(), NULL);
			return 0;
		}
		for(size_t i2 = 0; i2 < i; i2++) {
			for(size_t i3 = 0; i3 <= i; i3++) {
				if(i2 != i3) {
					mstruct[i2].replace(vargs[1][i3], mstruct[i3]);
				}
			}
		}
	}

	return 1;

}

MathStructure *find_deqn(MathStructure &mstruct);
MathStructure *find_deqn(MathStructure &mstruct) {
	if(mstruct.isFunction() && mstruct.function() == CALCULATOR->f_diff) return &mstruct;
	for(size_t i = 0; i < mstruct.size(); i++) {
		MathStructure *m = find_deqn(mstruct[i]);
		if(m) return m;
	}
	return NULL;
}

bool contains_ignore_diff(const MathStructure &m, const MathStructure &mstruct, const MathStructure &mdiff);
bool contains_ignore_diff(const MathStructure &m, const MathStructure &mstruct, const MathStructure &mdiff) {
	if(m.equals(mstruct)) return true;
	if(m.equals(mdiff)) return false;
	for(size_t i = 0; i < m.size(); i++) {
		if(contains_ignore_diff(m[i], mstruct, mdiff)) return true;
	}
	if(m.isVariable() && m.variable()->isKnown()) {
		return contains_ignore_diff(((KnownVariable*) m.variable())->get(), mstruct, mdiff);
	} else if(m.isVariable()) {
		if(mstruct.isNumber() || !m.representsNumber()) return true;
	} else if(m.isAborted()) {
		return true;
	}
	return false;
}

void add_C(MathStructure &m_eqn, const MathStructure &m_x, const MathStructure &m_y, const MathStructure &x_value, const MathStructure &y_value) {
	if(!y_value.isUndefined() && !x_value.isUndefined()) {
		MathStructure m_c(m_eqn);
		m_c.replace(m_x, x_value);
		m_c.replace(m_y, y_value);
		m_c.setType(STRUCT_ADDITION);
		m_c[1].negate();
		m_c.childUpdated(2);
		m_eqn[1] += m_c;
	} else {
		m_eqn[1] += CALCULATOR->v_C;
	}
	m_eqn.childrenUpdated();
}

bool dsolve(MathStructure &m_eqn, const EvaluationOptions &eo, const MathStructure &m_diff, const MathStructure &y_value, const MathStructure &x_value) {
	MathStructure m_y(m_diff[0]), m_x(m_diff[1]);
	bool b = false;
	if(m_eqn[0] == m_diff) {
		if(m_eqn[1].containsRepresentativeOf(m_y, true, true) == 0) {
			// y'=f(x)
			MathStructure m_fx(m_eqn[1]);
			if(m_fx.integrate(m_x, eo, true, false) > 0) {
				m_eqn[0] = m_y;
				m_eqn[1] = m_fx;
				b = true;
			}
		} else if(m_eqn[1].containsRepresentativeOf(m_x, true, true) == 0) {
			MathStructure m_fy(m_eqn[1]);
			m_fy.inverse();
			if(m_fy.integrate(m_y, eo, true, false) > 0) {
				m_eqn[0] = m_fy;
				m_eqn[1] = m_x;
				b = true;
			}
		} else if(m_eqn[1].isMultiplication() && m_eqn[1].size() >= 2) {
			b = true;
			MathStructure m_fx(1, 1, 0), m_fy(1, 1, 0);
			for(size_t i = 0; i < m_eqn[1].size(); i++) {
				if(m_eqn[1][i].containsRepresentativeOf(m_y, true, true) != 0) {
					if(m_eqn[1][i].containsRepresentativeOf(m_x, true, true) != 0) {
						b = false;
						break;
					}
					if(m_fy.isOne()) m_fy = m_eqn[1][i];
					else m_fy.multiply(m_eqn[1][i], true);
				} else {
					if(m_fx.isOne()) m_fx = m_eqn[1][i];
					else m_fx.multiply(m_eqn[1][i], true);
				}
			}
			if(b) {
				// y'=f(x)*f(y)
				m_fy.inverse();
				if(m_fy.integrate(m_y, eo, true, false)  > 0 && m_fx.integrate(m_x, eo, true, false) > 0) {
					m_eqn[0] = m_fy;
					m_eqn[1] = m_fx;
				} else {
					b = false;
				}
			}
		} else {
			MathStructure mfactor(m_eqn);
			mfactor[1].factorize(eo, false, 0, 0, false, false, NULL, m_x);
			if(mfactor[1].isMultiplication() && mfactor[1].size() >= 2) {
				mfactor.childUpdated(2);
				if(dsolve(mfactor, eo, m_diff, y_value, x_value)) {
					m_eqn = mfactor;
					return 1;
				}
			}
			if(m_eqn[1].isAddition()) {
				MathStructure m_left;
				MathStructure m_muly;
				MathStructure m_mul_exp;
				MathStructure m_exp;
				b = true;
				for(size_t i = 0; i < m_eqn[1].size(); i++) {
					if(m_eqn[1][i] == m_y) {
						if(m_muly.isZero()) m_muly = m_one;
						else m_muly.add(m_one, true);
					} else if(m_eqn[1][i].containsRepresentativeOf(m_y, true, true) != 0) {
						if(m_left.isZero() && m_eqn[1][i].isPower() && m_eqn[1][i][0] == m_y && (m_mul_exp.isZero() || m_eqn[1][i][1] == m_exp)) {
							if(m_mul_exp.isZero()) {
								m_exp = m_eqn[1][i][1];
								m_mul_exp = m_one;
							} else {
								m_mul_exp.add(m_one, true);
							}
						} else if(m_eqn[1][i].isMultiplication()) {
							size_t i_my = 0;
							bool b2 = false, b2_exp = false;
							for(size_t i2 = 0; i2 < m_eqn[1][i].size(); i2++) {
									if(!b2 && m_eqn[1][i][i2] == m_y) {
									i_my = i2;
									b2 = true;
								} else if(!b2 && m_left.isZero() && m_eqn[1][i][i2].isPower() && m_eqn[1][i][i2][0] == m_y && (m_mul_exp.isZero() || m_eqn[1][i][i2][1] == m_exp)) {
									i_my = i2;
									b2 = true;
									b2_exp = true;
								} else if(m_eqn[1][i][i2].containsRepresentativeOf(m_y, true, true) != 0) {
									b2 = false;
									break;
								}
							}
							if(b2) {
								MathStructure m_a(m_eqn[1][i]);
								m_a.delChild(i_my + 1, true);
								if(b2_exp) {
									if(m_mul_exp.isZero()) {
										m_exp = m_eqn[1][i][i_my][1];
										m_mul_exp = m_a;
									} else {
										m_mul_exp.add(m_a, true);
									}
								} else {
									if(m_muly.isZero()) m_muly = m_a;
									else m_muly.add(m_a, true);
								}
							} else {
								b = false;
								break;
							}
						} else {
							b = false;
							break;
						}
					} else {
						if(!m_mul_exp.isZero()) {
							b = false;
							break;
						}
						if(m_left.isZero()) m_left = m_eqn[1][i];
						else m_left.add(m_eqn[1][i], true);
					}
				}
				if(b && !m_muly.isZero()) {
					if(!m_mul_exp.isZero()) {
						if(m_exp.isOne() || !m_left.isZero()) return false;
						// y' = f(x)*y+g(x)*y^c
						b = false;
						m_muly.calculateNegate(eo);
						MathStructure m_y1_integ(m_muly);
						if(m_y1_integ.integrate(m_x, eo, true, false) > 0) {
							m_exp.negate();
							m_exp += m_one;
							MathStructure m_y1_exp(m_exp);
							m_y1_exp *= m_y1_integ;
							m_y1_exp.transform(STRUCT_POWER, CALCULATOR->v_e);
							m_y1_exp.swapChildren(1, 2);
							MathStructure m_y1_exp_integ(m_y1_exp);
							m_y1_exp_integ *= m_mul_exp;
							if(m_y1_exp_integ.integrate(m_x, eo, true, false) > 0) {
								m_eqn[1] = m_exp;
								m_eqn[1] *= m_y1_exp_integ;
								m_eqn[0] = m_y;
								m_eqn[0] ^= m_exp;
								m_eqn[0] *= m_y1_exp;
								add_C(m_eqn, m_x, m_y, x_value, y_value);
								m_eqn[0].delChild(m_eqn[0].size());
								m_eqn[1] /= m_y1_exp;
								m_eqn.childrenUpdated();
								return 1;
							}
						}
					} else if(m_left.isZero()) {
						// y'=f(x)*y+g(x)*y
						MathStructure mtest(m_eqn);
						MathStructure m_fy(m_y);
						m_fy.inverse();
						MathStructure m_fx(m_muly);
						if(m_fy.integrate(m_y, eo, true, false) > 0 && m_fx.integrate(m_x, eo, true, false) > 0) {
							m_eqn[0] = m_fy;
							m_eqn[1] = m_fx;
							b = true;
						}
					} else {
						// y'=f(x)*y+g(x)
						MathStructure integ_fac(m_muly);
						integ_fac.negate();
						if(integ_fac.integrate(m_x, eo, true, false) > 0) {
							UnknownVariable *var = new UnknownVariable("", "u");
							Assumptions *ass = new Assumptions();
							if(false) {
								ass->setType(ASSUMPTION_TYPE_REAL);
							}
							var->setAssumptions(ass);
							MathStructure m_u(var);
							m_u.inverse();
							if(m_u.integrate(var, eo, false, false) > 0) {
								MathStructure m_eqn2(integ_fac);
								m_eqn2.transform(COMPARISON_EQUALS, m_u);
								m_eqn2.isolate_x(eo, var);
								if(m_eqn2.isComparison() && m_eqn2.comparisonType() == COMPARISON_EQUALS && m_eqn2[0] == var) {
									integ_fac = m_eqn2[1];
									MathStructure m_fx(m_left);
									m_fx *= integ_fac;
									if(m_fx.integrate(m_x, eo, true, false) > 0) {
										m_eqn[0] = m_y;
										m_eqn[0] *= integ_fac;
										m_eqn[1] = m_fx;
										add_C(m_eqn, m_x, m_y, x_value, y_value);
										m_eqn[0] = m_y;
										m_eqn[1] /= integ_fac;
										m_eqn.childrenUpdated();
										return 1;
									}
								} else if(m_eqn2.isLogicalOr() && m_eqn2.size() >= 2) {
									b = true;
									for(size_t i = 0; i < m_eqn2.size(); i++) {
										if(!m_eqn2[i].isComparison() || m_eqn2[i].comparisonType() != COMPARISON_EQUALS || m_eqn2[i][0] != var) {
											b = false;
											break;
										}
									}
									if(b) {
										MathStructure m_eqn_new;
										m_eqn_new.setType(STRUCT_LOGICAL_OR);
										for(size_t i = 0; i < m_eqn2.size(); i++) {
											integ_fac = m_eqn2[i][1];
											MathStructure m_fx(m_left);
											m_fx *= integ_fac;
											if(m_fx.integrate(m_x, eo, true, false) > 0) {
												MathStructure m_fy(m_y);
												m_fy *= integ_fac;
												m_eqn_new.addChild(m_fy);
												m_eqn_new.last().transform(COMPARISON_EQUALS, m_fx);
												add_C(m_eqn_new.last(), m_x, m_y, x_value, y_value);
												m_eqn_new.last()[0] = m_y;
												m_eqn_new.last()[1] /= integ_fac;
												m_eqn_new.last().childrenUpdated();
												m_eqn_new.childUpdated(m_eqn_new.size());
											} else {
												b = false;
												break;
											}
										}
										if(b) {
											m_eqn_new.childrenUpdated();
											m_eqn = m_eqn_new;
											return 1;
										}
									}
								}
							}
							var->destroy();
						}
					}
				} else {
					b = false;
				}
			}
		}
		if(b) {
			add_C(m_eqn, m_x, m_y, x_value, y_value);
			return 1;
		}
	}
	return false;
}

DSolveFunction::DSolveFunction() : MathFunction("dsolve", 1, 3) {
	setDefaultValue(2, "undefined");
	setDefaultValue(3, "0");
}
int DSolveFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	MathStructure m_eqn(vargs[0]);
	EvaluationOptions eo2 = eo;
	eo2.isolate_x = false;
	eo2.protected_function = CALCULATOR->f_diff;
	m_eqn.eval(eo2);
	MathStructure *m_diff_p = NULL;
	if(m_eqn.isLogicalAnd()) {
		for(size_t i = 0; i < m_eqn.size(); i++) {
			if(m_eqn[i].isComparison() && m_eqn.comparisonType() == COMPARISON_EQUALS) {
				m_diff_p = find_deqn(m_eqn[i]);
				if(m_diff_p) break;
			}
		}
	} else if(m_eqn.isComparison() && m_eqn.comparisonType() == COMPARISON_EQUALS) {
		m_diff_p = find_deqn(m_eqn);
	}
	if(!m_diff_p) {
		CALCULATOR->error(true, _("No differential equation found."), NULL);
		mstruct = m_eqn; return -1;
	}
	MathStructure m_diff(*m_diff_p);
	if(m_diff.size() < 3 || (!m_diff[0].isSymbolic() && !m_diff[0].isVariable()) || (!m_diff[1].isSymbolic() && !m_diff[1].isVariable()) || !m_diff[2].isInteger() || !m_diff[2].number().isPositive() || !m_diff[2].number().isLessThanOrEqualTo(10)) {
		CALCULATOR->error(true, _("No differential equation found."), NULL);
		mstruct = m_eqn; return -1;
	}
	if(m_diff[2].number().intValue() != 1) {
		CALCULATOR->error(true, _("Unable to solve differential equation."), NULL);
		mstruct = m_eqn;
		return -1;
	}
	m_eqn.isolate_x(eo2, m_diff);
	mstruct = m_eqn;
	if(eo.approximation == APPROXIMATION_TRY_EXACT) eo2.approximation = APPROXIMATION_EXACT;
	if(m_eqn.isLogicalAnd()) {
		for(size_t i = 0; i < m_eqn.size(); i++) {
			if(m_eqn[i].isComparison() && m_eqn[i].comparisonType() == COMPARISON_EQUALS && m_eqn[i][0] == m_diff) {
				dsolve(m_eqn[i], eo2, m_diff, vargs[1], vargs[2]);
			}
		}
	} else if(m_eqn.isLogicalOr()) {
		for(size_t i = 0; i < m_eqn.size(); i++) {
			if(m_eqn[i].isComparison() && m_eqn[i].comparisonType() == COMPARISON_EQUALS && m_eqn[i][0] == m_diff) {
				dsolve(m_eqn[i], eo2, m_diff, vargs[1], vargs[2]);
			} else if(m_eqn[i].isLogicalAnd()) {
				for(size_t i2 = 0; i2 < m_eqn[i].size(); i2++) {
					if(m_eqn[i][i2].isComparison() && m_eqn[i][i2].comparisonType() == COMPARISON_EQUALS && m_eqn[i][i2][0] == m_diff) {
						dsolve(m_eqn[i][i2], eo2, m_diff, vargs[1], vargs[2]);
					}
				}
			}
		}
	} else if(m_eqn.isComparison() && m_eqn.comparisonType() == COMPARISON_EQUALS && m_eqn[0] == m_diff) {
		dsolve(m_eqn, eo2, m_diff, vargs[1], vargs[2]);
	}
	if(m_eqn.contains(m_diff)) {
		CALCULATOR->error(true, _("Unable to solve differential equation."), NULL);
		return -1;
	}
	m_eqn.calculatesub(eo2, eo2, true);
	MathStructure msolve(m_eqn);
	if(solve_equation(msolve, m_eqn, m_diff[0], eo, true, m_diff[1], MathStructure(CALCULATOR->v_C), vargs[2], vargs[1]) <= 0) {
		CALCULATOR->error(true, _("Unable to solve differential equation."), NULL);
		return -1;
	}
	mstruct = msolve;
	return 1;
}

LimitFunction::LimitFunction() : MathFunction("limit", 2, 4) {
	NumberArgument *arg = new NumberArgument("", ARGUMENT_MIN_MAX_NONE, false, false);
	arg->setComplexAllowed(false);
	arg->setHandleVector(true);
	setArgumentDefinition(2, arg);
	setArgumentDefinition(3, new SymbolicArgument());
	setDefaultValue(3, "x");
	IntegerArgument *iarg = new IntegerArgument();
	iarg->setMin(&nr_minus_one);
	iarg->setMax(&nr_one);
	setArgumentDefinition(4, iarg);
	setDefaultValue(4, "0");
}
int LimitFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {
	if(vargs[1].isVector()) return 0;
	mstruct = vargs[0];
	EvaluationOptions eo2 = eo;
	eo2.approximation = APPROXIMATION_EXACT;
	if(mstruct.calculateLimit(vargs[2], vargs[1], eo2, vargs[3].number().intValue())) return 1;
	CALCULATOR->error(true, _("Unable to find limit."), NULL);
	return -1;
}

PlotFunction::PlotFunction() : MathFunction("plot", 1, 6) {
	NumberArgument *arg = new NumberArgument();
	arg->setComplexAllowed(false);
	arg->setHandleVector(false);
	setArgumentDefinition(2, arg);
	setDefaultValue(2, "0");
	arg = new NumberArgument();
	arg->setHandleVector(false);
	arg->setComplexAllowed(false);
	setArgumentDefinition(3, arg);
	setDefaultValue(3, "10");
	setDefaultValue(4, "1001");
	setArgumentDefinition(5, new SymbolicArgument());
	setDefaultValue(5, "x");
	setArgumentDefinition(6, new BooleanArgument());
	setDefaultValue(6, "0");
	setCondition("\\y < \\z");
}
int PlotFunction::calculate(MathStructure &mstruct, const MathStructure &vargs, const EvaluationOptions &eo) {

	EvaluationOptions eo2;
	eo2.parse_options = eo.parse_options;
	eo2.approximation = APPROXIMATION_APPROXIMATE;
	eo2.parse_options.read_precision = DONT_READ_PRECISION;
	eo2.interval_calculation = INTERVAL_CALCULATION_NONE;
	bool use_step_size = vargs[5].number().getBoolean();
	mstruct = vargs[0];
	CALCULATOR->beginTemporaryStopIntervalArithmetic();
	if(!mstruct.contains(vargs[4], true)) {
		mstruct.eval(eo2);
	} else {
		eo2.calculate_functions = false;
		eo2.expand = false;
		CALCULATOR->beginTemporaryStopMessages();
		mstruct.eval(eo2);
		int im = 0;
		if(CALCULATOR->endTemporaryStopMessages(NULL, &im) > 0 || im > 0) mstruct = vargs[0];
		eo2.calculate_functions = eo.calculate_functions;
		eo2.expand = eo.expand;
	}
	CALCULATOR->endTemporaryStopIntervalArithmetic();
	vector<MathStructure> x_vectors, y_vectors;
	vector<PlotDataParameters*> dpds;
	if(mstruct.isMatrix() && mstruct.columns() == 2) {
		MathStructure x_vector, y_vector;
		mstruct.columnToVector(1, x_vector);
		mstruct.columnToVector(2, y_vector);
		y_vectors.push_back(y_vector);
		x_vectors.push_back(x_vector);
		PlotDataParameters *dpd = new PlotDataParameters;
		dpd->title = _("Matrix");
		dpds.push_back(dpd);
	} else if(mstruct.isVector()) {
		int matrix_index = 1, vector_index = 1;
		if(mstruct.size() > 0 && (mstruct[0].isVector() || mstruct[0].contains(vargs[4], false, true, true))) {
			for(size_t i = 0; i < mstruct.size() && !CALCULATOR->aborted(); i++) {
				MathStructure x_vector;
				if(mstruct[i].isMatrix() && mstruct[i].columns() == 2) {
					MathStructure y_vector;
					mstruct[i].columnToVector(1, x_vector);
					mstruct[i].columnToVector(2, y_vector);
					y_vectors.push_back(y_vector);
					x_vectors.push_back(x_vector);
					PlotDataParameters *dpd = new PlotDataParameters;
					dpd->title = _("Matrix");
					if(matrix_index > 1) {
						dpd->title += " ";
						dpd->title += i2s(matrix_index);
					}
					matrix_index++;
					dpds.push_back(dpd);
				} else if(mstruct[i].isVector()) {
					y_vectors.push_back(mstruct[i]);
					x_vectors.push_back(x_vector);
					PlotDataParameters *dpd = new PlotDataParameters;
					dpd->title = _("Vector");
					if(vector_index > 1) {
						dpd->title += " ";
						dpd->title += i2s(vector_index);
					}
					vector_index++;
					dpds.push_back(dpd);
				} else {
					MathStructure y_vector;
					if(use_step_size) {
						CALCULATOR->beginTemporaryStopMessages();
						CALCULATOR->beginTemporaryStopIntervalArithmetic();
						y_vector.set(mstruct[i].generateVector(vargs[4], vargs[1], vargs[2], vargs[3], &x_vector, eo2));
						CALCULATOR->endTemporaryStopIntervalArithmetic();
						CALCULATOR->endTemporaryStopMessages();
						if(y_vector.size() == 0) CALCULATOR->error(true, _("Unable to generate plot data with current min, max and step size."), NULL);
					} else if(!vargs[3].isInteger() || !vargs[3].representsPositive()) {
						CALCULATOR->error(true, _("Sampling rate must be a positive integer."), NULL);
					} else {
						bool overflow = false;
						int steps = vargs[3].number().intValue(&overflow);
						CALCULATOR->beginTemporaryStopMessages();
						CALCULATOR->beginTemporaryStopIntervalArithmetic();
						if(steps <= 1000000 && !overflow) y_vector.set(mstruct[i].generateVector(vargs[4], vargs[1], vargs[2], steps, &x_vector, eo2));
						CALCULATOR->endTemporaryStopIntervalArithmetic();
						CALCULATOR->endTemporaryStopMessages();
						if(y_vector.size() == 0) CALCULATOR->error(true, _("Unable to generate plot data with current min, max and sampling rate."), NULL);
					}
					if(CALCULATOR->aborted()) {
						mstruct.clear();
						return 1;
					}
					if(y_vector.size() > 0) {
						x_vectors.push_back(x_vector);
						y_vectors.push_back(y_vector);
						PlotDataParameters *dpd = new PlotDataParameters;
						MathStructure mprint;
						if(vargs[0].isVector() && vargs[0].size() == mstruct.size()) mprint = vargs[0][i];
						else mprint = mstruct[i];
						dpd->title = format_and_print(mprint);
						dpd->test_continuous = true;
						dpds.push_back(dpd);
					}
				}
			}
		} else {
			MathStructure x_vector;
			y_vectors.push_back(mstruct);
			x_vectors.push_back(x_vector);
			PlotDataParameters *dpd = new PlotDataParameters;
			dpd->title = _("Vector");
			dpds.push_back(dpd);
		}
	} else {
		MathStructure x_vector, y_vector;
		if(use_step_size) {
			CALCULATOR->beginTemporaryStopMessages();
			CALCULATOR->beginTemporaryStopIntervalArithmetic();
			y_vector.set(mstruct.generateVector(vargs[4], vargs[1], vargs[2], vargs[3], &x_vector, eo2));
			CALCULATOR->endTemporaryStopIntervalArithmetic();
			CALCULATOR->endTemporaryStopMessages();
			if(y_vector.size() == 0) CALCULATOR->error(true, _("Unable to generate plot data with current min, max and step size."), NULL);
		} else if(!vargs[3].isInteger() || !vargs[3].representsPositive()) {
			CALCULATOR->error(true, _("Sampling rate must be a positive integer."), NULL);
		} else {
			bool overflow = false;
			int steps = vargs[3].number().intValue(&overflow);
			CALCULATOR->beginTemporaryStopMessages();
			CALCULATOR->beginTemporaryStopIntervalArithmetic();
			if(steps <= 1000000 && !overflow) y_vector.set(mstruct.generateVector(vargs[4], vargs[1], vargs[2], steps, &x_vector, eo2));
			CALCULATOR->endTemporaryStopIntervalArithmetic();
			CALCULATOR->endTemporaryStopMessages();
			if(y_vector.size() == 0) CALCULATOR->error(true, _("Unable to generate plot data with current min, max and sampling rate."), NULL);
		}
		if(CALCULATOR->aborted()) {
			mstruct.clear();
			return 1;
		}
		if(y_vector.size() > 0) {
			x_vectors.push_back(x_vector);
			y_vectors.push_back(y_vector);
			PlotDataParameters *dpd = new PlotDataParameters;
			MathStructure mprint(vargs[0]);
			dpd->title = format_and_print(mprint);
			dpd->test_continuous = true;
			dpds.push_back(dpd);
		}
	}
	if(x_vectors.size() > 0 && !CALCULATOR->aborted()) {
		PlotParameters param;
		CALCULATOR->plotVectors(&param, y_vectors, x_vectors, dpds, false, 0);
		for(size_t i = 0; i < dpds.size(); i++) {
			if(dpds[i]) delete dpds[i];
		}
	}
	mstruct.clear();
	return 1;

}

