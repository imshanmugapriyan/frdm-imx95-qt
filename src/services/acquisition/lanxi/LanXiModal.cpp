#include "LanXiModal.h"
#include "LanXiAdvanced.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace lanxi {
namespace {
using C=std::complex<double>;using V=std::vector<C>;
constexpr double pi=3.14159265358979323846;
void need(bool ok,const char*s){if(!ok)throw std::runtime_error(s);}
double energy(const V &v){double e=0;for(auto x:v)e+=std::norm(x);return e;}
C dot(const V&a,const V&b){C sum=0;for(std::size_t i=0;i<a.size();++i)sum+=std::conj(a[i])*b[i];return sum;}
struct Evaluation {double cost=0;std::vector<double> residual;std::vector<V> fitted,coefficients;};
Evaluation evaluate(const std::vector<double>&hz,const std::vector<V>&frfs,const std::vector<double>&p,bool residuals){
    const auto modes=p.size()/2,n=hz.size(),columns=modes+(residuals?2:0);std::vector<V> basis(columns,V(n)),q;std::vector<double> scales(columns);std::vector<V> r(columns,V(columns));
    for(std::size_t j=0;j<modes;++j){const double wn=2*pi*std::exp(p[2*j]),z=std::exp(p[2*j+1]);for(std::size_t k=0;k<n;++k){const double w=2*pi*hz[k];basis[j][k]=1.0/C(wn*wn-w*w,2*z*wn*w);}}
    if(residuals)for(std::size_t k=0;k<n;++k){basis[modes][k]=1;basis[modes+1][k]=1/(hz[k]*hz[k]);}
    for(std::size_t j=0;j<columns;++j){scales[j]=std::sqrt(energy(basis[j]));need(scales[j]>0&&std::isfinite(scales[j]),"Singular modal basis.");V v=basis[j];for(auto &x:v)x/=scales[j];
        for(int pass=0;pass<2;++pass)for(std::size_t i=0;i<j;++i){auto coefficient=dot(q[i],v);r[i][j]+=coefficient;for(std::size_t k=0;k<n;++k)v[k]-=coefficient*q[i][k];}
        const double norm=std::sqrt(energy(v));need(norm>1e-10,"Modal terms are indistinguishable; revise seeds or frequency range.");r[j][j]=norm;for(auto &x:v)x/=norm;q.push_back(std::move(v));
    }
    Evaluation result;
    for(const auto &h:frfs){V coefficients(columns);for(std::size_t j=0;j<columns;++j)coefficients[j]=dot(q[j],h);
        for(std::size_t j=columns;j-->0;){for(std::size_t k=j+1;k<columns;++k)coefficients[j]-=r[j][k]*coefficients[k];coefficients[j]/=r[j][j];}
        for(std::size_t j=0;j<columns;++j)coefficients[j]/=scales[j];V fit(n);const double normalization=std::sqrt(energy(h));
        for(std::size_t k=0;k<n;++k){for(std::size_t j=0;j<columns;++j)fit[k]+=basis[j][k]*coefficients[j];auto e=(h[k]-fit[k])/normalization;result.cost+=std::norm(e);result.residual.push_back(e.real());result.residual.push_back(e.imag());}
        result.fitted.push_back(std::move(fit));result.coefficients.push_back(std::move(coefficients));
    }
    return result;
}
std::vector<double> solve(std::vector<std::vector<double>> a,std::vector<double>b){const auto n=b.size();for(std::size_t k=0;k<n;++k){std::size_t pivot=k;for(std::size_t j=k+1;j<n;++j)if(std::abs(a[j][k])>std::abs(a[pivot][k]))pivot=j;need(std::abs(a[pivot][k])>1e-20,"Modal optimizer is singular.");std::swap(a[k],a[pivot]);std::swap(b[k],b[pivot]);for(std::size_t i=k+1;i<n;++i){double f=a[i][k]/a[k][k];for(std::size_t j=k;j<n;++j)a[i][j]-=f*a[k][j];b[i]-=f*b[k];}}std::vector<double>x(n);for(std::size_t i=n;i-->0;){double v=b[i];for(std::size_t j=i+1;j<n;++j)v-=a[i][j]*x[j];x[i]=v/a[i][i];}return x;}
}
ModalModel fitModes(const std::vector<double>&hz,const std::vector<V>&frfs,const std::vector<double>&seeds,bool residualTerms,const std::function<bool()>&cancelled){
    need(!seeds.empty()&&seeds.size()<=6&&hz.size()>=32&&hz.size()<=2048&&hz.size()>8*seeds.size(),"Fit requires 1..6 mode seeds and 32..2048 frequency lines (more than 8 per mode).");
    need(!frfs.empty()&&frfs.size()<=32,"Modal fit accepts 1..32 response functions.");
    for(std::size_t k=0;k<hz.size();++k)need(std::isfinite(hz[k])&&hz[k]>0&&(!k||hz[k]>hz[k-1]),"Modal frequencies must be positive and strictly increasing.");
    for(const auto &h:frfs){need(h.size()==hz.size()&&energy(h)>1e-300&&std::isfinite(energy(h)),"Invalid modal response data.");for(auto v:h)need(std::isfinite(v.real())&&std::isfinite(v.imag()),"Non-finite modal response.");}
    std::vector<double> parameters,lower,upper;
    for(std::size_t j=0;j<seeds.size();++j){need(std::isfinite(seeds[j])&&seeds[j]>hz.front()&&seeds[j]<hz.back()&&(!j||seeds[j]>seeds[j-1]),"Mode seeds must increase inside the fit range.");
        lower.push_back(std::log(j?(seeds[j-1]+seeds[j])/2:hz.front()));upper.push_back(std::log(j+1<seeds.size()?(seeds[j]+seeds[j+1])/2:hz.back()));parameters.push_back(std::log(seeds[j]));parameters.push_back(std::log(.02));}
    Evaluation current=evaluate(hz,frfs,parameters,residualTerms);double lambda=.001;ModalModel model;
    for(int iteration=0;iteration<100;++iteration){if(cancelled)need(!cancelled(),"Modal fit cancelled.");model.iterations=iteration+1;if(current.cost<1e-20){model.converged=true;break;}
        const auto n=parameters.size();std::vector<std::vector<double>> jac(n),normal(n,std::vector<double>(n));std::vector<double> rhs(n);
        for(std::size_t j=0;j<n;++j){auto p=parameters;p[j]+=1e-5;auto shifted=evaluate(hz,frfs,p,residualTerms);jac[j].resize(current.residual.size());for(std::size_t k=0;k<jac[j].size();++k)jac[j][k]=(shifted.residual[k]-current.residual[k])/1e-5;}
        for(std::size_t j=0;j<n;++j){for(std::size_t k=0;k<current.residual.size();++k)rhs[j]-=jac[j][k]*current.residual[k];for(std::size_t l=0;l<n;++l)for(std::size_t k=0;k<current.residual.size();++k)normal[j][l]+=jac[j][k]*jac[l][k];normal[j][j]+=lambda*std::max(1e-12,normal[j][j]);}
        auto step=solve(normal,rhs),trial=parameters;for(std::size_t j=0;j<n;++j){trial[j]+=std::clamp(step[j],j%2?-.7:-.2,j%2?.7:.2);trial[j]=std::clamp(trial[j],j%2?std::log(.0001):lower[j/2],j%2?std::log(.3):upper[j/2]);}
        auto next=evaluate(hz,frfs,trial,residualTerms);
        if(next.cost<current.cost){double improvement=current.cost-next.cost;parameters=trial;current=std::move(next);lambda=std::max(1e-9,lambda*.3);if(improvement<1e-10*std::max(current.cost,1e-10)){model.converged=true;break;}}
        else {lambda*=10;if(lambda>1e12)break;}
    }
    model.objective=current.cost;model.fitted=current.fitted;
    for(std::size_t j=0;j<seeds.size();++j){Mode mode;mode.frequency=std::exp(parameters[2*j]);mode.damping=std::exp(parameters[2*j+1]);for(auto &c:current.coefficients)mode.residues.push_back(c[j]);model.modes.push_back(mode);}
    for(std::size_t c=0;c<frfs.size();++c){V residual(hz.size());for(std::size_t k=0;k<hz.size();++k)residual[k]=frfs[c][k]-model.fitted[c][k];model.relativeError.push_back(std::sqrt(energy(residual)/energy(frfs[c])));model.frac.push_back(modalAssurance(frfs[c],model.fitted[c]));}
    return model;
}
}
