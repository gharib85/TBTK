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

/** @file ArnoldiSolver.cpp
 *
 *  @author Kristofer Björnson
 */

/* Note: The ArnoldiSolver is based on the ARPACK driver zndrv2.f, which can
 * be found at EXAMPLES/COMPLEX/zndrv2.f in the ARPACK source tree. The main
 * loop closely resembles zndrv2.f.
 * ARPACK can be downloaded from http://www.caam.rice.edu/software/ARPACK/.
 * See http://www.caam.rice.edu/software/ARPACK/UG/node138.html for more
 * information about parameters. */

#include "../../../include/Solvers/ArnoldiSolver/ArnoldiSolver.h"
#include "Streams.h"
#include "TBTKMacros.h"

#include <iostream>

using namespace std;

namespace TBTK{

ArnoldiSolver::ArnoldiSolver(){
	mode = Mode::Normal;

	//Arnoldi variables (ARPACK)
	calculateEigenVectors = false;
	numEigenValues = 0;
	numLanczosVectors = 0;
	shift = 0.;
	tolerance = 0.;
	maxIterations = 20;
	residuals = NULL;
	eigenValues = NULL;
	eigenVectors = NULL;
}

ArnoldiSolver::~ArnoldiSolver(){
	//Free Arnoldi variables (ARPACK)
	if(residuals != NULL)
		delete [] residuals;
	if(eigenValues != NULL)
		delete [] eigenValues;
	if(eigenVectors != NULL)
		delete [] eigenVectors;
}

//ARPACK function for performing single Arnoldi iteration step
extern "C" void znaupd_(
        int                     *IDO,
        char                    *BMAT,
        int                     *N,
        char                    *WHICH,
        int                     *NEV,
        double                  *TOL,
        complex<double>         *RESID,
        int                     *NCV,
        complex<double>         *V,
        int                     *LDV,
        int                     *IPARAM,
        int                     *IPNTR,
        complex<double>         *WORKD,
        complex<double>         *WORKL,
        int                     *LWORKL,
        double                  *RWORK,
        int                     *INFO
);

//ARPACK function for extracting calculated eigenvalues and eigenvectors
extern "C" void zneupd_(
        int                     *RVEC,
        char                    *HOWMANY,
        int                     *SELECT,
        complex<double>         *D,
        complex<double>         *Z,
        int                     *LDZ,
        complex<double>         *sigma,
        complex<double>         *WORKev,
        char                    *BMAT,
        int                     *N,
        char                    *WHICH,
        int                     *NEV,
        double                  *TOL,
        complex<double>         *RESID,
        int                     *NCV,
        complex<double>         *V,
        int                     *LDV,
        int                     *IPARAM,
        int                     *IPNTR,
        complex<double>         *WORKD,
        complex<double>         *WORKL,
        int                     *LWORKL,
        double                  *RWORK,
        int                     *INFO
);

void ArnoldiSolver::run(){
	Streams::out << "Running ArnoldiSolver.\n";

	switch(mode){
	case Mode::Normal:
		initNormal();
		arnoldiLoop();
		break;
	case Mode::ShiftAndInvert:
		initShiftAndInvert();
		arnoldiLoop();
		break;
	default:
		TBTKExit(
			"ArnoldiSolver::run()",
			"Unknown mode.",
			"This should never happen, contact the developer."
		);
	}
	sort();
}

void ArnoldiSolver::arnoldiLoop(){
	TBTKAssert(
		numEigenValues > 0,
		"ArnoldiSolver::arnoldiLoop()",
		"The number of eigenvalues must be larger than 0.",
		""
	);
	TBTKAssert(
		numLanczosVectors >= numEigenValues + 2,
		"ArnoldiSolver::arnoldiLoop()",
		"The number of Lanczos vectors must be at least two larger"
		<< " than the number of eigenvalues (" << numEigenValues
		<< ").",
		""
	);

	const Model &model = getModel();
	int basisSize = model.getBasisSize();

	//I = Standard eigenvalue problem Ax = lambda*x
	char bmat[1] = {'I'};
	//Which Ritz value of operator to compute, LM = compute the
	//numEigenValues largest (in magnitude) eigenvalues.
	char which[2] = {'L', 'M'};

	complex<double> sigma = shift;

	//Reverse communication variable.
	int ido = 0;
	//info=0 indicates that a random vector is used to start the Arnoldi
	//iteration.
	int info = 0;

	//Integer parameters used by ARPACK
	int iparam[11];
	//Exact shifts with respect to the current Hessenberg matrix
	iparam[0] = 1;
	//Maximum number of Arnoldi iterations
	iparam[2] = maxIterations;
	switch(mode){
	case Mode::Normal:
		//Use mode 1 of ZNAUPD	(normal matrix multiplication)
		iparam[6] = 1;
		break;
	case Mode::ShiftAndInvert:
		//Use mode 3 of ZNAUPD	(shift and invert)
		iparam[6] = 3;
		break;
	default:
		TBTKExit(
			"ArnoldiSolver::arnoldiLoop()",
			"Unknown mode.",
			"This should never happen, contact the developer."
		);
	}

	//Integer "pointer" used by ARPACK to index into workd
	int ipntr[14];

	//Allocate workspaces and output
	int worklSize = 3*numLanczosVectors*numLanczosVectors + 5*numLanczosVectors;
	residuals = new complex<double>[basisSize];
	complex<double> *lanczosVectors = new complex<double>[basisSize*numLanczosVectors];
	complex<double> *workd = new complex<double>[3*basisSize];
	complex<double> *workl = new complex<double>[worklSize];
	double *rwork = new double[basisSize];
	int *select = new int[numLanczosVectors];	//Need to be allocated, but not initialized as long as howMany = 'A' in call to zneupd_
	eigenValues = new complex<double>[numEigenValues];
	if(calculateEigenVectors)
		eigenVectors = new complex<double>[numEigenValues*model.getBasisSize()];
	complex<double> *workev = new complex<double>[2*numLanczosVectors];

	//Only used in Mode::ShiftAndInvert.
	Matrix<complex<double>> b(basisSize, 1);

	//Main loop ()
	int counter = 0;
	while(true){
		Streams::out << "." << flush;
		if(counter%10 == 9)
			Streams::out << " ";
		if(counter%50 == 49)
			Streams::out << "\n";

		TBTKAssert(
			counter++ <= maxIterations,
			"ArnoldiSolver::arnoldiLoop()",
			"Maximum number of iterations reached.",
			""
		);

		//Calculate one more Lanczos vector
		znaupd_(
			&ido,
			bmat,
			&basisSize,
			which,
			&numEigenValues,
			&tolerance,
			residuals,
			&numLanczosVectors,
			lanczosVectors,
			&basisSize,
			iparam,
			ipntr,
			workd,
			workl,
			&worklSize,
			rwork,
			&info
		);

		checkZnaupdInfo(info);
/*		if(info != 0){
			if(info == 1){
				TBTKExit(
					"ArnoldiSolver::arnoldiLoop()",
					"Maximum number of iterations"
					<< " reached.",
					""
				);
			}
			else if(info == 3){
				TBTKExit(
					"ArnoldiSolver::arnoldiLoop()",
					"No shifts could be applied during"
					<< " implicit Arnoldi update.",
					"Try increasing the number of Lanczos"
					<< " vectors."
				);
			}
			else if(info == -9999){
				TBTKExit(
					"ArnoldiSolver::arnoldiLoop()",
					"Unable to build an Arnoldi"
					<< " factorization.",
					"Likely input error to znaupd, which"
					<< " should never happen, contact the"
					<< " developer."
				);
			}
			else if(info < 0){
				TBTKExit(
					"ArnoldiSolver::arnoldiLoop()",
					"Input parameter '" << -info << "' to"
					<< " znaupd is invalid.",
					"This should never happen, contact the"
					<< " developer."
				);
			}
			else{
				TBTKExit(
					"ArnoldiSolver::arnoldiLoop()",
					"znaupd() exited with unknown error"
					<< " info = " << info << "'.",
					"This should never happen, contact the"
					<< " developer."
				);
			}
		}*/

		if(
			executeReverseCommunicationMessage(
				ido,
				basisSize,
				workd,
				ipntr,
				b
			)
		){
			break;
		}
/*		if(ido == -1 || ido == 1){
			switch(mode){
			case Mode::Normal:
			{
				//Perform matrix multiplcation y = Ax, where x =
				//workd[ipntr[0]] and y = workd[ipntr[1]]. "-1" is for
				//conversion between Fortran one based indices and c++
				//zero based indices.
				for(int n = 0; n < basisSize; n++)
					workd[(ipntr[1] - 1) + n] = 0.;

				int numMatrixElements = model.getHoppingAmplitudeSet()->getNumMatrixElements();
				const int *cooRowIndices = model.getHoppingAmplitudeSet()->getCOORowIndices();
				const int *cooColIndices = model.getHoppingAmplitudeSet()->getCOOColIndices();
				const complex<double> *cooValues = model.getHoppingAmplitudeSet()->getCOOValues();
				for(int n = 0; n < numMatrixElements; n++)
					workd[(ipntr[1] - 1) + cooRowIndices[n]] += cooValues[n]*workd[(ipntr[0] - 1) + cooColIndices[n]];

				break;
			}
			case Mode::ShiftAndInvert:
				//Solve x = (A - sigma*I)^{-1}b, where b =
				//workd[ipntr[0]] and x = workd[ipntr[1]]. "-1"
				//is for conversion between Fortran one based
				//indices and c++ zero based indices.
				for(int n = 0; n < basisSize; n++)
					b.at(n, 0) = workd[(ipntr[0] - 1) + n];

				luSolver.solve(b);

				for(int n = 0; n < basisSize; n++)
					workd[(ipntr[1] - 1) + n] = b.at(n, 0);

				break;
			default:
				TBTKExit(
					"ArnoldiSolver::arnoldiLoop()",
					"Unknown mode.",
					"This should never happen, contact the"
					<< " developer."
				);
			}
		}
		else if(ido == 99){
			break;
		}
		else{
			TBTKExit(
				"ArnoldiSolver::arnoldiLoop()",
				"znaupd returned with ido = '" << ido << "',"
				<< " which is not supported.",
				"This should never happen, contact the"
				<< " developer."
			);
		}*/
	}
	Streams::out << "\n";

	//A = Compute numberOfEigenValues Ritz vectors
	char howMany = 'A';
	//Error message (Set to the same value as info
	int ierr = info;
	//Convert flag from bool to int
	int calculateEigenVectorsBool = calculateEigenVectors;

	//Extract eigenvalues and eigenvectors
	zneupd_(
		&calculateEigenVectorsBool,
		&howMany,
		select,
		eigenValues,
		eigenVectors,
		&basisSize,
		&sigma,
		workev,
		bmat,
		&basisSize,
		which,
		&numEigenValues,
		&tolerance,
		residuals,
		&numLanczosVectors,
		lanczosVectors,
		&basisSize,
		iparam,
		ipntr,
		workd,
		workl,
		&worklSize,
		rwork,
		&ierr
	);

	if(ierr < 0){
		TBTKExit(
			"ArnoldiSolver::arnoldiLoop()",
			"Input parameter '" << -info << "' to zneupd is"
			<< " invalid.",
			"This should never happen, contact the developer."
		);
	}
	else if(ierr != 0){
		TBTKExit(
			"ArnoldiSolver::arnoldiLoop()",
			"zneupd() exited with error ierr = " << ierr
			<< ". Unknown error ().",
			"This should never happen, contact the developer."
		);
		//The documentation for zneupd() says the following in case
		//ierr = 1:
		//
		//The Schur form computed by LAPACK routine csheqr could not be
		//reordered by LAPACK routine ztrsen. Re-enter subroutine
		//ZNEUPD with IPARAM(5)=NCV and increase the size of the array
		//D to have dimension at least dimension NCV and allocate at
		//least NCV columns for Z. NOTE: Not necessary if Z and V share
		//the same space. Please notify the authors if this error
		//occurs.
	}

	double numAccurateEigenValues = iparam[4]; //With respect to tolerance
	Streams::out << "\nNumber of accurately converged eigenvalues: "
		<< numAccurateEigenValues << "\n";
	//Calculate |Ax - lambda*x| here
	//...

	//Free memory
	delete [] lanczosVectors;
	delete [] workd;
	delete [] workl;
	delete [] rwork;
	delete [] select;
	delete [] workev;
}

void ArnoldiSolver::checkZnaupdInfo(int info) const{
	if(info != 0){
		if(info == 1){
			TBTKExit(
				"ArnoldiSolver::arnoldiLoop()",
				"Maximum number of iterations"
				<< " reached.",
				""
			);
		}
		else if(info == 3){
			TBTKExit(
				"ArnoldiSolver::arnoldiLoop()",
				"No shifts could be applied during"
				<< " implicit Arnoldi update.",
				"Try increasing the number of Lanczos"
				<< " vectors."
			);
		}
		else if(info == -9999){
			TBTKExit(
				"ArnoldiSolver::arnoldiLoop()",
				"Unable to build an Arnoldi"
				<< " factorization.",
				"Likely input error to znaupd, which"
				<< " should never happen, contact the"
				<< " developer."
			);
		}
		else if(info < 0){
			TBTKExit(
				"ArnoldiSolver::arnoldiLoop()",
				"Input parameter '" << -info << "' to"
				<< " znaupd is invalid.",
				"This should never happen, contact the"
				<< " developer."
			);
		}
		else{
			TBTKExit(
				"ArnoldiSolver::arnoldiLoop()",
				"znaupd() exited with unknown error"
				<< " info = " << info << "'.",
				"This should never happen, contact the"
				<< " developer."
			);
		}
	}
}

bool ArnoldiSolver::executeReverseCommunicationMessage(
	int ido,
	int basisSize,
	complex<double> *workd,
	int *ipntr,
	Matrix<complex<double>> &b
){
	if(ido == -1 || ido == 1){
		switch(mode){
		case Mode::Normal:
		{
			//Perform matrix multiplcation y = Ax, where x =
			//workd[ipntr[0]] and y = workd[ipntr[1]]. "-1" is for
			//conversion between Fortran one based indices and c++
			//zero based indices.
			for(int n = 0; n < basisSize; n++)
				workd[(ipntr[1] - 1) + n] = 0.;

			const Model &model = getModel();
			int numMatrixElements = model.getHoppingAmplitudeSet()->getNumMatrixElements();
			const int *cooRowIndices = model.getHoppingAmplitudeSet()->getCOORowIndices();
			const int *cooColIndices = model.getHoppingAmplitudeSet()->getCOOColIndices();
			const complex<double> *cooValues = model.getHoppingAmplitudeSet()->getCOOValues();
			for(int n = 0; n < numMatrixElements; n++)
				workd[(ipntr[1] - 1) + cooRowIndices[n]] += cooValues[n]*workd[(ipntr[0] - 1) + cooColIndices[n]];

			break;
		}
		case Mode::ShiftAndInvert:
			//Solve x = (A - sigma*I)^{-1}b, where b =
			//workd[ipntr[0]] and x = workd[ipntr[1]]. "-1"
			//is for conversion between Fortran one based
			//indices and c++ zero based indices.
			for(int n = 0; n < basisSize; n++)
				b.at(n, 0) = workd[(ipntr[0] - 1) + n];

			luSolver.solve(b);

			for(int n = 0; n < basisSize; n++)
				workd[(ipntr[1] - 1) + n] = b.at(n, 0);

			break;
		default:
			TBTKExit(
				"ArnoldiSolver::arnoldiLoop()",
				"Unknown mode.",
				"This should never happen, contact the"
				<< " developer."
			);
		}
	}
	else if(ido == 99){
		return true;
	}
	else{
		TBTKExit(
			"ArnoldiSolver::arnoldiLoop()",
			"znaupd returned with ido = '" << ido << "',"
			<< " which is not supported.",
			"This should never happen, contact the"
			<< " developer."
		);
	}

	return false;
}

void ArnoldiSolver::initNormal(){
	//Get matrix representation on COO format
	const Model &model = getModel();
	const int *cooRowIndices = model.getHoppingAmplitudeSet()->getCOORowIndices();
	const int *cooColIndices = model.getHoppingAmplitudeSet()->getCOOColIndices();
	TBTKAssert(
		cooRowIndices != NULL && cooColIndices != NULL,
		"ArnoldiSolver::initNormal()",
		"COO format not constructed.",
		"Use Model::constructCOO() to construct COO format."
	);
}

void ArnoldiSolver::initShiftAndInvert(){
	const Model &model = getModel();

	SparseMatrix<complex<double>> matrix(
		SparseMatrix<complex<double>>::StorageFormat::CSC
	);

	HoppingAmplitudeSet::Iterator it = getModel().getHoppingAmplitudeSet(
	)->getIterator();
	const HoppingAmplitude *ha;
	while((ha = it.getHA())){
		int from = model.getHoppingAmplitudeSet()->getBasisIndex(
			ha->getFromIndex()
		);
		int to = model.getHoppingAmplitudeSet()->getBasisIndex(
			ha->getToIndex()
		);
		matrix.add(to, from, ha->getAmplitude());

		it.searchNextHA();
	}
	for(int n = 0; n < model.getBasisSize(); n++)
		matrix.add(n, n, -shift);
	matrix.constructCSX();

	luSolver.setMatrix(matrix);
}

void ArnoldiSolver::sort(){
	complex<double> *workspace = new complex<double>[numEigenValues];
	for(int n = 0; n < numEigenValues; n++)
		workspace[n] = eigenValues[n];

	int *order = new int[numEigenValues];
	int *orderWorkspace = new int[numEigenValues];
	for(int n = 0; n < numEigenValues; n++){
		order[n] = n;
		orderWorkspace[n] = n;
	}

	mergeSortSplit(
		workspace,
		eigenValues,
		orderWorkspace,
		order,
		0,
		numEigenValues
	);

	const Model &model = getModel();
	if(calculateEigenVectors){
		complex<double> *eigenVectorsWorkspace = new complex<double>[numEigenValues*model.getBasisSize()];
		for(int n = 0; n < numEigenValues*model.getBasisSize(); n++)
			eigenVectorsWorkspace[n] = eigenVectors[n];

		for(int n = 0; n < numEigenValues; n++)
			for(int c = 0; c < model.getBasisSize(); c++)
				eigenVectors[n*model.getBasisSize() + c] = eigenVectorsWorkspace[order[n]*model.getBasisSize() + c];

		delete [] eigenVectorsWorkspace;
	}

	delete [] order;
	delete [] orderWorkspace;
	delete [] workspace;
}

void ArnoldiSolver::mergeSortSplit(
	complex<double> *dataIn,
	complex<double> *dataOut,
	int *orderIn,
	int *orderOut,
	int first,
	int end
){
	if(end - first < 2)
		return;

	int middle = (end + first)/2;

	mergeSortSplit(dataOut, dataIn, orderOut, orderIn, first, middle);
	mergeSortSplit(dataOut, dataIn, orderOut, orderIn, middle, end);

	mergeSortMerge(dataIn, dataOut, orderIn, orderOut, first, middle, end);
}

void ArnoldiSolver::mergeSortMerge(
	complex<double> *dataIn,
	complex<double> *dataOut,
	int *orderIn,
	int *orderOut,
	int first,
	int middle,
	int end
){
	int i = first;
	int j = middle;
	for(int k = first; k < end; k++){
		if(i < middle && (j >= end || real(dataIn[i]) <= real(dataIn[j]))){
			dataOut[k] = dataIn[i];
			orderOut[k] = orderIn[i];
			i++;
		}
		else{
			dataOut[k] = dataIn[j];
			orderOut[k] = orderIn[j];
			j++;
		}
	}
}

};	//End of namespace TBTK
