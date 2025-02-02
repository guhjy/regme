#ifndef __CONSTMATRIX__H__
#define __CONSTMATRIX__H__


#include "operatorMatrix.h"
#include "error_check.h"

using namespace std;



constMatrix::~constMatrix()
{
	for(int i = 0; i < nop; i++)
	  Q[i].resize(0,0);
	delete[] Q;
}

void constMatrix::initFromList(Rcpp::List const & init_list)
{
  npars  = 1;
 std::vector<std::string> check_names =  {"Q","loc", "h"};
 check_Rcpplist(init_list, check_names, "constMatrix::initFromList");
	dtau_old = 0;
 tau = 1.;
 out_list = clone(init_list);
 if(init_list.containsElementNamed("tau"))
   tau = Rcpp::as<double >( init_list["tau"]);
  Rcpp::List Q_list  = Rcpp::as<Rcpp::List> (init_list["Q"]);
  Rcpp::List loc_list  = Rcpp::as<Rcpp::List> (init_list["loc"]);
  Rcpp::List h_list  = Rcpp::as<Rcpp::List> (init_list["h"]);
  nop = Q_list.size();
  Q = new Eigen::SparseMatrix<double,0,int>[nop];

  d.resize(nop);
  loc.resize(nop);
  h.resize(nop);
  h_average.resize(nop);
  m_loc.resize(nop);

  for(int i=0;i<nop;i++){
      //SEXP tmp = Q_list[i];
      Q[i] =  Rcpp::as<Eigen::SparseMatrix<double,0,int>>(Q_list[i]);
      d[i] = Q[i].rows();
      Q[i] *= tau;
      loc[i]  = Rcpp::as< Eigen::VectorXd >( loc_list[i]);
      h[i]  = Rcpp::as< Eigen::VectorXd >(h_list[i]);
      h_average[i] = h[i].sum() / h[i].size();
      m_loc[i] = loc[i].minCoeff();
  }

  int nIter = 1;
  if(init_list.containsElementNamed("nIter"))
    nIter = Rcpp::as<int>(init_list["nIter"]);
  tauVec.resize(nIter+1);
  v.setZero(1);
  m.resize(1,1);

  dtau  = 0.;
  ddtau = 0.;
  counter = 0;
  tauVec[counter] = tau;
  counter++;
}

void constMatrix::initFromList(Rcpp::List const & init_List, Rcpp::List const & solver_list)
{
  this->initFromList(init_List);
}

void constMatrix::gradient_init(int nsim, int nrep)
{
  dtau   = 0;
  ddtau  = 0;
}
Eigen::MatrixXd constMatrix::d2Given( const Eigen::VectorXd & X,
                   const Eigen::VectorXd & iV,
                   const Eigen::VectorXd & mean_KX,
                  int ii,
                  const double weight)
{
  if(nop == 1)
    ii = 0;
  Eigen::VectorXd vtmp = Q[ii] * X;

  double xtQx =  vtmp.dot( iV.asDiagonal() * vtmp);
  Eigen::MatrixXd d2 = Eigen::MatrixXd::Zero(1,1);
  d2(0, 0) = -weight * (d[ii] + xtQx ) / pow(tau, 2);

  return(d2);

}
void constMatrix::gradient_add( const Eigen::VectorXd & X,
								   const Eigen::VectorXd & iV,
								   const Eigen::VectorXd & mean_KX,
								  int ii,
								  const double weight)
{
	if(nop == 1)
		ii = 0;

  Eigen::VectorXd KX = Q[ii]*X;
  dtau    += weight * d[ii]/tau;
  ddtau   += weight * (-d[ii]/pow(tau,2));
  Eigen::VectorXd dKX = Q[ii] * X/tau;
  dtau -=  weight * dKX.dot(iV.asDiagonal() * (KX - mean_KX)); 
  ddtau -= weight * (dKX.dot(iV.asDiagonal() * dKX));

}

void constMatrix::gradient( const Eigen::VectorXd & X, const Eigen::VectorXd & iV)
{
  throw(" constMatrix::gradient depricated \n");
}

void constMatrix::print_parameters(){
  Rcpp::Rcout << "tau = " << tau ;
}

void constMatrix::step_theta(const double stepsize,
							 const double learning_rate,
							 const double polyak_rate,
							 const int burnin)
{
	dtau  /= ddtau;
  	dtau_old = learning_rate * dtau_old + dtau;
  	double step = stepsize * dtau_old;
	double tau_temp = -1.;
    while(tau_temp < 0)
    {
    	step *= 0.5;
        tau_temp = tau - step;
    }

  for(int i=0;i<nop;i++){
    Q[i] *= tau_temp/tau;
  }

	tau = tau_temp;

	if(counter == 0 || polyak_rate == -1)
		tauVec[counter] = tau;
	else
		tauVec[counter] = polyak_rate * tau + (1 - polyak_rate) * tauVec[counter-1];

  //Rcpp::Rcout << "tauVec[" << counter << "] = " << tauVec[counter] << "\n";
	counter++;
	clear_gradient();
	ddtau  = 0;
}

Rcpp::List constMatrix::output_list()
{
  out_list["tau"] = tauVec(tauVec.size() -1 );
  out_list["tauVec"] = tauVec;
  out_list["nIter"] = tauVec.size();
  out_list["Cov_theta"]   = Cov_theta;
  return(out_list);
}


double constMatrix::trace_variance( const Eigen::SparseMatrix<double,0,int>& A, int i){

  if( nop == 1)
    i = 0;

  Eigen::SparseMatrix<double,0,int> Qt = Q[i].transpose();
  Eigen::SparseMatrix<double,0,int> At = A.transpose();
  Eigen::SparseLU< Eigen::SparseMatrix<double,0,int> > chol(Qt);
  Eigen::MatrixXd QtAt = chol.solve(At);
  Eigen::MatrixXd QQ = QtAt*h[i].asDiagonal()*QtAt.transpose();
  return(QQ.trace());
}


#endif
