/////////////////////////////////////////////////////////////////////////////
/// Name:        unsteadyTracerFlow.cpp
/// Purpose:     methods related to tracer flow simulation
///              in pore/throat networks are defined here.
/// Author:      Ahmed Hamdi Boujelben <ahmed.hamdi.boujelben@gmail.com>
/// Created:     2017
/// Copyright:   (c) 2018 Ahmed Hamdi Boujelben
/// Licence:     Attribution-NonCommercial 4.0 International
/////////////////////////////////////////////////////////////////////////////

#include "network.h"
#include "iterator.h"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace PNM {

void network::runTracerModel()
{
    cout<<"Starting Tracer Flow Model... "<<endl;

    //post-processing
    if(videoRecording)
        record=true;

    initialiseTracerModel();

    //initialise flags
    double timeSoFar(0);
    double injectedPVs(0);

    //Check oil connectivity
    clusterOilElements();

    if (isOilSpanning){
        //Solve pressure in Oil
        solvePressureFieldInOil();

        //Set timestep
        calculateTracerTimeStep();
    }
    else{
        simulationInterrupted=true;
        cout<<"ERROR: Tracer will only flow in the oil. Oil is not spanning in the current configuration."<<endl;
    }

    //Define working concentration vector to avoid frequent allocations
    vector<double> newConcentration(totalPores + totalNodes);

    while(!simulationInterrupted && timeSoFar<simulationTime)
    {       
        updateConcentrationValues(newConcentration);

        timeSoFar+=timeStep;
        injectedPVs+=timeStep*flowRate/totalNetworkVolume;

        //Display notification
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "Injected PVs: " << injectedPVs;
        simulationNotification = ss.str();
        emitUpdateNotificationSignal();

        //Update graphics
        emitPlotSignal();

        //Thread Management
        if(simulationInterrupted)break;
    }

    //Update graphics
    emitPlotSignal();

    //post-processing
    if(videoRecording)
    {
        record=false;
        extractVideo();
    }

    cout<<"Simulation Time: "<<timeSoFar<<" s"<<endl;
    cout<<"Injected PVs: "<<injectedPVs<<endl; 
}

void network::initialiseTracerModel()
{
    simulationInterrupted=false;
    if(waterDistribution!=4){ //not after primary drainage
        assignWWWettability();
        fillWithPhase(phase::water,initialWaterSaturation,waterDistribution,phase::oil);
    }
    else{ //after primary drainage
        initialiseTwoPhaseSSModel();
        primaryDrainage(initialWaterSaturation);
    }

    initialiseCapillaries();

    restoreWettability();

    if(overrideByInjectedPVs){
        simulationTime=totalNetworkVolume*injectedPVs/flowRate;
        cout<<"PVs to inject: "<<injectedPVs<<endl;
    }
}

void network::solvePressureFieldInOil()
{
    //Assign fluid properties and deactivate non-flowing capillaries (i.e. water, non-spanning oil)
    assignViscosities();
    assignConductivities();
    for (pore* p :networkRange<pore*>(this))
    {
        p->setActive(true);
        if(p->getPhaseFlag()==phase::water){
           p->setActive(false);
        }
        if((p->getPhaseFlag()==phase::oil && !p->getClusterOil()->getSpanning()) || (p->getNodeIn()!=0 && p->getNodeIn()->getPhaseFlag()==phase::water) || (p->getNodeOut()!=0 && p->getNodeOut()->getPhaseFlag()==phase::water)){
            p->setActive(false);
        }
    }

    clusterActiveElements();
    for(pore* p : networkRange<pore*>(this)){
        if(p->getActive() && p->getClusterActive()->getSpanning()==false){
            p->setCapillaryPressure(0);
            p->setActive(false);
        }
    }

    //Link pressure gradient to flow rate (Aker,1998)
    double Q1(0),Q2(0),A,B;

    pressureIn=1;
    pressureOut=0;
    solvePressures();
    updateFlows();
    Q1=getOutletFlow();

    pressureIn=2;
    pressureOut=0;
    solvePressures();
    updateFlows();
    Q2=getOutletFlow();

    B=(Q1-Q2*1/2)/(1-1/2);
    A=(Q1-B);

    deltaP=flowRate/A-B/A;

    pressureIn=deltaP;
    pressureOut=0;
    solvePressures();
    updateFlows();
}

void network::calculateTracerTimeStep()
{
    timeStep=1e50;

    for(pore* p : networkRange<pore*>(this))
    {
        if(p->getPhaseFlag()==phase::oil && p->getClusterOil()->getSpanning())
        {
            //Diffusion
            double sumDiffusionSource=0;
            for(element*e : p->getNeighboors())
            {
                if(e->getPhaseFlag()==phase::oil)
                {
                    double area=min(e->getVolume()/e->getLength(), p->getVolume()/p->getLength());
                    sumDiffusionSource+=tracerDiffusionCoef/area;
                }
            }

            //Convection
            if((abs(p->getFlow())/p->getVolume()+sumDiffusionSource)>1e-24)
            {
                double step=1./(abs(p->getFlow())/p->getVolume()+sumDiffusionSource);
                if(step<timeStep)
                {
                    timeStep=step;
                }
            }

            if(p->getFlow()>1e-24 && p->getActive())
            {
                node* n=p->getNodeIn();
                if(n!=0)
                {
                    n->setFlow((n->getFlow()+abs(p->getFlow())));
                }
            }
            if(p->getFlow()<-1e-24 && p->getActive())
            {
                node* n=p->getNodeOut();
                if(n!=0)
                {
                    n->setFlow((n->getFlow()+abs(p->getFlow())));
                }
            }
        }
    }

    for(node* p : networkRange<node*>(this))
    {
        if(p->getPhaseFlag()==phase::oil && p->getClusterOil()->getSpanning())
        {
            //Diffusion
            double sumDiffusionSource=0;
            for(element*e : p->getNeighboors())
            {
                if(e->getPhaseFlag()==phase::oil)
                {
                    double area=min(e->getVolume()/e->getLength(), p->getVolume()/p->getLength());
                    sumDiffusionSource+=tracerDiffusionCoef/area;
                }
            }

            //Convection
            if((abs(p->getFlow())/p->getVolume()+sumDiffusionSource)>1e-24)
            {
                double step=1./(abs(p->getFlow())/p->getVolume()+sumDiffusionSource);
                if(step<timeStep)
                {
                    timeStep=step;
                }
            }
        }
    }
}

void network::updateConcentrationValues(vector<double> &newConcentration)
{
    for(node* n: networkRange<node*>(this))
    {
        if(n->getPhaseFlag()==phase::oil  && n->getClusterOil()->getSpanning())
        {
            //Convection
            double massIn=0;
            for(element* e : n->getNeighboors())
            {
                pore* p= static_cast<pore*>(e);
                if(p->getPhaseFlag()==phase::oil && p->getActive())
                {
                    if((p->getNodeIn()==n && p->getFlow()>1e-24) || (p->getNodeOut()==n && p->getFlow()<-1e-24))
                    {
                        massIn+=p->getConcentration()*abs(p->getFlow());
                    }
                }
            }
            n->setMassFlow(massIn);

            //Diffusion
            double sumDiffusionIn=0;
            double sumDiffusionOut=0;
            for(element*e : n->getNeighboors())
            {
                if(e->getPhaseFlag()==phase::oil)
                {
                    double area=min(e->getVolume()/e->getLength(), n->getVolume()/n->getLength());
                    sumDiffusionIn+=e->getConcentration()*tracerDiffusionCoef/area;
                    sumDiffusionOut+=n->getConcentration()*tracerDiffusionCoef/area;
                }
            }

            //Load new concentration in a temporary vector
            newConcentration[n->getAbsId()]=(n->getConcentration()+(massIn-abs(n->getFlow())*n->getConcentration())*timeStep/n->getVolume()+sumDiffusionIn*timeStep-sumDiffusionOut*timeStep);
        }
    }

    for(pore* p : networkRange<pore*>(this))
    {
        if(p->getPhaseFlag()==phase::oil  && p->getClusterOil()->getSpanning())
        {
            double massIn=0;
            double flowIn=0;
            double sumDiffusionIn=0;
            double sumDiffusionOut=0;

            //Convection
            if(p->getInlet())
            {
                if(abs(p->getFlow())>1e-24 && p->getActive())
                {
                    massIn=abs(p->getFlow());
                    flowIn=abs(p->getFlow());
                }
            }
            else if(p->getOutlet()){
                if(abs(p->getFlow())>1e-24 && p->getActive())
                {
                    node* activeNode = p->getNodeIn() == 0? p->getNodeOut() : p->getNodeIn();
                    massIn=activeNode->getMassFlow();
                    flowIn=activeNode->getFlow();
                }
            }
            else{
                if(p->getFlow()>1e-24 && p->getActive() && p->getNodeOut()->getPhaseFlag()==phase::oil)
                {
                    massIn=p->getNodeOut()->getMassFlow();
                    flowIn=p->getNodeOut()->getFlow();
                }
                if(p->getFlow()<1e-24 && p->getActive() && p->getNodeIn()->getPhaseFlag()==phase::oil)
                {
                    massIn=p->getNodeIn()->getMassFlow();
                    flowIn=p->getNodeIn()->getFlow();
                }
            }

            if(abs(p->getFlow())<1e-24 || flowIn<1e-24 || !p->getActive())
            {
                massIn=0;
                flowIn=1;
            }

            //Diffusion
            for(element*e : p->getNeighboors())
            {
                if(e->getPhaseFlag()==phase::oil)
                {
                    double area=min(e->getVolume()/e->getLength(), p->getVolume()/p->getLength());
                    sumDiffusionIn+=e->getConcentration()*tracerDiffusionCoef/area;
                    sumDiffusionOut+=p->getConcentration()*tracerDiffusionCoef/area;
                }
            }

            //Load new concentration in a temporary vector
            newConcentration[p->getAbsId()]=(p->getConcentration()+(abs(p->getFlow())/flowIn*massIn-abs(p->getFlow())*p->getConcentration())*timeStep/p->getVolume()+sumDiffusionIn*timeStep-sumDiffusionOut*timeStep);
        }
    }

    //Update concentrations
    for(element* e: networkRange<element*>(this))
    {
        if(e->getPhaseFlag()==phase::oil && e->getClusterOil()->getSpanning())
        {
            e->setConcentration(newConcentration[e->getAbsId()]);
            if(e->getConcentration()<-0.00001 || e->getConcentration()>1.0001)
            {
                simulationInterrupted=true;
                cout<<"ERROR: Concentration out of range: "<< e->getConcentration()<<endl;
            }
        }
    }
}

}
