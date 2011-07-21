#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include "particle.h"
#include "main.h"
#include "gravity.h"

// Wisdom Holman Integrator
// for non-rotating frames.
// Central mass is fixed at r=0 and has mass=1
// Based on the swift code.

void drift_wh(double _dt);
void drift_dan(struct particle* pv, double dt, int* iflag);
void drift_kepu(double dt, double r0, double mu, double alpha, double u, double* fp, double* c1, double* c2, double* c3, int* iflag);
void drift_kepu_guess(double dt0, double r0, double mu, double alpha, double u, double* s);
void drift_kepu_p3solve(double dt0, double r0, double mu, double alpha, double u, double* s, int* iflag);
void drift_kepu_new(double* s, double dt0, double r0, double mu, double alpha, double u, double* fp, double* c1, double* c2, double* c3, int* iflag);
void drift_kepu_lag(double* s, double dt0, double r0, double mu, double alpha, double u, double* fp, double* c1, double* c2, double* c3, int* iflag);
void drift_kepu_stumpff(double x, double* c0, double* c1, double* c2, double* c3);
void drift_kepu_fchk(double dt0, double r0, double mu, double alpha, double u, double s, double* f);
void drift_kepmd(double dm, double es, double ec, double* x, double* s, double* c);

int WH_SELFGRAVITY_ENABLED = 1;

void integrate_particles(){
	if (WH_SELFGRAVITY_ENABLED==1){
		// DRIFT
		drift_wh(dt/2.);
		// KICK
		calculate_forces();
		for (int i=1;i<N;i++){
			//					   indirect term
			/*
			particles[i].vx += dt * particles[i].ax;
			particles[i].vy += dt * particles[i].ay;
			particles[i].vz += dt * particles[i].az;
			*/
			particles[i].vx += dt * (particles[i].ax - 1.*particles[0].ax);
			particles[i].vy += dt * (particles[i].ay - 1.*particles[0].ay);
			particles[i].vz += dt * (particles[i].az - 1.*particles[0].az);
		}

		// DRIFT
		drift_wh(dt/2.);
	}else{
		// DRIFT
		drift_wh(dt);
	}
}

void drift_wh(double _dt){
#pragma omp parallel for
	for (int i=1;i<N;i++){
		struct particle* p = &(particles[i]);
		int iflag = 0;
		drift_dan(p,_dt,&iflag);
		if (iflag != 0){ // Try again with 10 times smaller timestep.
			for (int j=0;j<10;j++){
				drift_dan(&(particles[i]),_dt/10.,&iflag);
				if (iflag != 0) return;
			}
		}
	}
}

void drift_dan(struct particle* pv, double dt0, int* iflag){
	double dt1 = dt0;
	double x0 = pv->x-particles[0].x;
	double y0 = pv->y-particles[0].y;
	double z0 = pv->z-particles[0].z;
	double vx0 = pv->vx;
	double vy0 = pv->vy;
	double vz0 = pv->vz;
	
	double r0 = sqrt(x0*x0 + y0*y0 + z0*z0);
	double v0s = vx0*vx0 + vy0*vy0 + vz0*vz0;
	double u = x0*vx0 + y0*vy0 + z0*vz0;
	double mu = G*(particles[0].m+pv->m);
	double alpha = 2.0*mu/r0 - v0s;

	if (alpha > 0.0){
		double a = mu/alpha;
		double asq = a*a;
		double en = sqrt(mu/(a*asq));
		double ec = 1.0 - r0/a;
		double es = u/(en*asq);
		double esq = ec*ec + es*es;
		double dm = dt1*en - floor(dt1*en/(2.0*M_PI))*2.0*M_PI; // TODO: Check that floor = int in fortran
		dt1 = dm/en;
		if ((esq*dm*dm < 0.0016) && !(dm*dm > 0.16 || esq > 0.36) ){
			double s, c, xkep;
			drift_kepmd(dm,es,ec,&xkep,&s,&c);
			double fchk = (xkep - ec*s + es*(1.-c) - dm);
#define DANBYB 1.e-13
			if (fchk*fchk > DANBYB){
				*iflag =1;
				return;
			}
			
			double fp = 1. - ec*c + es*s;
			double f = (a/r0) * (c-1.) + 1.;
			double g = dt1 + (s-xkep)/en;
			double fdot = - (a/(r0*fp))*en*s;
			double gdot = (c-1.)/fp + 1.;
	
			pv->x = x0*f + vx0*g;
			pv->y = y0*f + vy0*g;
			pv->z = z0*f + vz0*g;
			
			pv->vx = x0*fdot + vx0*gdot;
			pv->vy = y0*fdot + vy0*gdot;
			pv->vz = z0*fdot + vz0*gdot;

			*iflag =0;
			return;
		}
	}
	double c1;
	double c2;
	double c3;
	double fp;
	drift_kepu(dt1,r0,mu,alpha,u,&fp,&c1,&c2,&c3,iflag);
	if (*iflag==0){
		double f = 1.0 - (mu/r0)*c2;
		double g = dt1 - mu*c3;
		double fdot = -(mu/(fp*r0))*c1;
		double gdot = 1.0 - (mu/fp)*c2;

		pv->x = x0*f + vx0*g + particles[0].x;
		pv->y = y0*f + vy0*g + particles[0].y;
		pv->z = z0*f + vz0*g + particles[0].z;
		
		pv->vx = x0*fdot + vx0*gdot;
		pv->vy = y0*fdot + vy0*gdot;
		pv->vz = z0*fdot + vz0*gdot;
	}
}


void drift_kepu(double dt0, double r0, double mu, double alpha, double u,
		double* fp, double* c1, double* c2, double* c3, int* iflag){
	// iflag == 0 if converged
	double s, st;
	drift_kepu_guess(dt0, r0, mu, alpha, u, &s);
	st = s;
	drift_kepu_new(&s,dt0,r0,mu,alpha,u,fp,c1,c2,c3,iflag);
	if (*iflag!=0){
		// fall back
		double fo,fn;
		drift_kepu_fchk(dt0,r0,mu,alpha,u,st,&fo);
		drift_kepu_fchk(dt0,r0,mu,alpha,u,s, &fn);
		if (fabs(fo)<fabs(fn)){
			s = st;
		}
		drift_kepu_lag(&s,dt0,r0,mu,alpha,u,fp,c1,c2,c3,iflag);
	}
}

void drift_kepu_guess(double dt0, double r0, double mu, double alpha, double u, double* s){
	if (alpha > 0.){
		// elliptic motion
		if ((dt0/r0) <= 0.4){
			*s = dt0/r0 - (dt0*dt0*u)/(2.*r0*r0*r0);
			return;
		}else{
			double a = mu/alpha;
			double en = sqrt(mu/(a*a*a));
			double ec = 1. - r0/a;
			double es = u/(en*a*a);
			double e = sqrt(ec*ec + es*es);
			double y = en*dt0 - es;
			double sy = sin(y);
			double cy = cos(y);
			double sigma = ( (es*cy + ec*sy)>=0 ? 1. : -1. ); 
			double x = y + sigma*0.85*e;
			*s = x/sqrt(alpha);
		}
	}else{
		// hyperbolic
		int iflag=0;
		drift_kepu_p3solve(dt0,r0,mu,alpha,u,s,&iflag);
		if (iflag!=0){
			*s = dt0/r0;
		}
	}
}

void drift_kepu_p3solve(double dt0, double r0, double mu, double alpha, double u, double* s, int* iflag){
	double denom = (mu - alpha*r0)/6.;
	double a2 = 0.5*u/denom;
	double a1 = r0/denom;
	double a0 = -dt0/denom;
	double q = (a1 - a2*a2/3.)/3.;
	double r = (a1*a2 - 3.*a0)/6. - a2*a2*a2/27.;
	double sq2 = q*q*q + r*r;

	if (sq2 >= 0.) {
		double sq = sqrt(sq2);
		double p1, p2;
		if ((r+sq) <= 0.){
			p1 = -pow(-(r+sq),1./3.);
		}else{
			p1 = pow(r+sq,1./3.);
		}
		if ((r-sq) <= 0.){
			p2 = -pow(-(r-sq),1./3.);
		}else{
			p2 = pow(r-sq,1./3.);
		}
		*iflag = 0;
		*s = p1 + p2 - a2/3.;
	}else{
		*iflag = 1;
		*s = 0;
	}
}

void drift_kepu_new(double* s, double dt0, double r0, double mu, double alpha, double u, double* fp, double* c1, double* c2, double* c3, int* iflag){
	int nc;
	for (nc=0;nc<6;nc++){
		double x = (*s)*(*s)*alpha;
		double c0;
		drift_kepu_stumpff(x,&c0,c1,c2,c3);
		*c1 = (*c1) * (*s);
		*c2 = (*c2) * (*s) * (*s);
		*c3 = (*c3) * (*s) * (*s) * (*s);
		double f = r0*(*c1) + u*(*c2) + mu*(*c3) - dt0;
		(*fp) = r0*c0 + u*(*c1) + mu*(*c2);
		double fpp = (-r0*alpha + mu)*(*c1) + u*c0;
		double fppp = (-r0*alpha + mu)*c0 - u*alpha*(*c1);
		double ds = -f/(*fp);
		ds = -f/((*fp) +ds*fpp/2.);
		ds = -f/((*fp) +ds*fpp/2.+ds*ds*fppp/6.);
		*s = (*s) + ds;
		double fdt = f/dt0;
		if (fdt*fdt < DANBYB*DANBYB){
			*iflag = 0;
			return;
		}
	}
	// Not converged
	*iflag = 1;
}

void drift_kepu_stumpff(double x, double* c0, double* c1, double* c2, double* c3){
	int n = 0;
	double xm = 0.1;
	while(fabs(x)>= xm){
		n++;
		x = x / 4.;
	}
	
	*c2 = (1.-x*(1.-x*(1.-x*(1.-x*(1.-x*(1.-x/182.)/132.)/90.)/56.)/30.)/12.)/2.;
	*c3 = (1.-x*(1.-x*(1.-x*(1.-x*(1.-x*(1.-x/210.)/156.)/110.)/72.)/42.)/20.)/6.;
	*c1 = 1. - x*(*c3);
	*c0 = 1. - x*(*c2);
	if (n!=0){  // TODO: This loop structure is overly complicated
		int i;
		for (i=n;i>=1;i--){
			*c3 = ((*c2) + (*c0)*(*c3))/4.;
			*c2 = (*c1)*(*c1)/2.;
			*c1 = (*c0)*(*c1);
			*c0 = 2.*(*c0)*(*c0) - 1.;
			x = x * 4.; 
		}
	}
}

void drift_kepu_fchk(double dt0, double r0, double mu, double alpha, double u, double s, double* f){
	double x = s*s*alpha;
	double c0, c1, c2, c3;
	drift_kepu_stumpff(x,&c0,&c1,&c2,&c3);
	c1 = c1 *s;
	c2 = c2 *s*s;
	c3 = c3 *s*s*s;
	*f = r0*c1 + u*c2 + mu*c3 - dt0;
}

void drift_kepu_lag(double* s, double dt0, double r0, double mu, double alpha, double u, double* fp, double* c1, double* c2, double* c3, int* iflag){
	int ncmax = 400;
	double ln = 5.;
	
	int nc;
	for (nc=0;nc<=ncmax;nc++){
		double x = (*s)*(*s)*alpha;
		double c0;
		drift_kepu_stumpff(x,&c0,c1,c2,c3);
		*c1 = (*c1)*(*s);
		*c2 = (*c2)*(*s)*(*s);
		*c3 = (*c3)*(*s)*(*s)*(*s);
		double f = r0*(*c1) + u*(*c2) + mu*(*c3) - dt0;
		(*fp) = r0*c0 + u*(*c1) + mu*(*c2);
		double fpp = (-40.*alpha + mu)*(*c1) + u*c0;
		double ds = -ln*f/((*fp) + ((*fp)>0.?1.:-1.)*sqrt(fabs((ln - 1.) * (ln - 1.) *(*fp)*(*fp) - (ln - 1.) * ln * f * fpp)));
		*s = (*s) + ds;

		double fdt = f/dt0;
		if (fdt*fdt < DANBYB*DANBYB){
			*iflag = 0;
			return;
		}
	}
	*iflag = 2;
}

void drift_kepmd(double dm, double es, double ec, double* x, double* s, double* c){
	const double A0 = 39916800.;
	const double A1 = 6652800.;
	const double A2 = 332640.;
	const double A3 = 7920.;
	const double A4 = 110.;

	double fac1 = 1./(1. -ec);
	double q = fac1 * dm;
	double fac2 = es*es*fac1 - ec/3.;
	*x = q*(1. - 0.5*fac1*q*(es-q*fac2));

	double y = (*x)*(*x);
	*s = (*x)*(A0-y*(A1-y*(A2-y*(A3-y*(A4-y)))))/A0;
	*c = sqrt(1. - (*s)*(*s));

	double f = (*x) - ec*(*s) + es*(1.-(*c)) -dm;
	double fp = 1. - ec*(*c) + es*(*s);
	double fpp = ec*(*s) + es*(*c);
	double fppp = ec*(*c) - es*(*s);
	double dx = -f/fp;
	dx = -f/(fp + 0.5*dx*fpp);
	dx = -f/(fp + 0.5*dx*fpp + 0.16666666666666666666*dx*dx*fppp);
	*x = (*x) + dx;

	y = (*x)*(*x);
	*s = (*x)*(A0-y*(A1-y*(A2-y*(A3-y*(A4-y)))))/A0;
	*c = sqrt(1. - (*s)*(*s));
}
