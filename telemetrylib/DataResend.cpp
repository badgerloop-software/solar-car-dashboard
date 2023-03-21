//
// Created by Mingcan Li on 2/23/23.
//

#include "DataResend.h"


void DataResend::setChannel(DTI *channel) {
    this->channel = channel;
}

void DataResend::addToQueue(QByteArray arr) {
    qDebug() << "Queued msg: "<<q.size()+1<<"\n";
    mutex.lock();
    q.enqueue(arr);
    mutex.unlock();
}

void DataResend::comStatus(bool state) {
    comstate = state;
    if(state && !q.empty()) {
        t = new std::thread(&DataResend::resend, this);
        t->detach();
    }
}

void DataResend::resend() {
    connect(this, SIGNAL(send(QByteArray)), channel, SLOT(sendData(QByteArray)));
    busy = true; //if DataResend is currently busy, Telemetry would not send data directly thru channel, and instead send it thru this class
    mutex.lock(); //use a mutex to stop queue access during resend
    qDebug()<<"sending "<<q.size()<<" packets";
    while (!q.empty()){
        qDebug()<<".";
        if (!comstate){
            disconnect(this, SIGNAL(send(QByteArray)), 0, 0);
            break;
        }
        mutex.unlock(); //additional data can be added during this temporary unlock
        mutex.lock();
        emit(send(q.dequeue()));
    }
    mutex.unlock(); //exit the loop data can be added
    disconnect(this, SIGNAL(send(QByteArray)), 0, 0);
    busy = false;
}
