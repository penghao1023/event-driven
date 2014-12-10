// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/**
 * \defgroup
 * @ingroup emorph_lib
 *
 * Data transport method for the eMorph project using the standard Bottle
 * format tailored for vEvents
 *
 * Author: Arren Glover 2014
 * Copyright (C) 2010 RobotCub Consortium
 * CopyPolicy: Released under the terms of the GNU GPL v2.0.
*/

#include <iCub/emorph/vBottle.h>


namespace emorph {

void vBottle::addEvent(emorph::vEvent &e) {

    //first append a searchable string
    //yarp::os::Bottle::addString(e.getType());
    yarp::os::Bottle * b = yarp::os::Bottle::find(e.getType()).asList();

    if(!b) {
        yarp::os::Bottle::addString(e.getType());
        b = &(yarp::os::Bottle::addList());
    }

    //add the coded event to the end of the bottle
    b->append(e.encode());

}

void vBottle::append(vBottle &eb)
{
    //we need to access the data in eb as if it were a normal bottle
    //so we cast it to a Bottle

    //TODO: just make sure the functions are available but protected should
    //      make the casting unnecessary
    yarp::os::Bottle * bb = dynamic_cast<yarp::os::Bottle *>(&eb);

    //for each list of events
    for(int tagi = 0; tagi < bb->size(); tagi+=2) {

        //get the appended event type
        const std::string tagname = bb->get(tagi).asString();
        if(!tagname.size()) {
            std::cerr << "Warning: Could not get tagname during vBottle append."
                         "Check vBottle integrity." << std::endl;
            continue;
        }

        //and the contents to append
        yarp::os::Bottle *b_from = bb->get(tagi+1).asList();
        if(b_from->size()) {
            std::cerr << "Warning: From-list empty during vBottle append."
                         "Check vBottle integrity." << std::endl;
            continue;
        }

        //get the correct bottle to append to (or create a new one)
        yarp::os::Bottle *b_to = yarp::os::Bottle::find(tagname).asList();
        if(!b_to) {
            yarp::os::Bottle::addString(tagname);
            b_to = &(yarp::os::Bottle::addList());
        }

        //and do it
        b_to->append(*b_from);
    }

}

void vBottle::getAll(emorph::vQueue &q)
{
    q.clear();

    for(int i = 0; i < Bottle::size(); i+=2) {
        vEvent * e = emorph::createEvent(Bottle::get(i).asString());
        if(!e) {
            std::cerr << "Warning: could not get bottle type during"
                         "getAllSorted. Check vBottle integrity." << std::endl;
            continue;
        }

        Bottle * b = Bottle::get(i+1).asList();

        int pos = 0;
        while(pos < b->size()) {
            q.push_back(e->decode(*b, pos));
        }
        delete(e);

    }
}

void vBottle::getAllSorted(emorph::vQueue &q)
{
    getAll(q);
    q.sort();

}


} //end namespace emorph