#include "vParticleFilter.h"
#include <cmath>
#include <limits>

using ev::event;
using ev::AddressEvent;

double generateGaussianNoise(double mu, double sigma)
{
    const double epsilon = std::numeric_limits<double>::min();
    const double two_pi = 2.0*3.14159265358979323846;

    static double z0, z1;
    static bool generate;
    generate = !generate;

    if (!generate)
       return z1 * sigma + mu;

    double u1, u2;
    do
     {
       u1 = rand() * (1.0 / RAND_MAX);
       u2 = rand() * (1.0 / RAND_MAX);
     }
    while ( u1 <= epsilon );

    z0 = sqrt(-2.0 * log(u1)) * cos(two_pi * u2);
    z1 = sqrt(-2.0 * log(u1)) * sin(two_pi * u2);
    return z0 * sigma + mu;
}

/*////////////////////////////////////////////////////////////////////////////*/
//VPARTICLETRACKER
/*////////////////////////////////////////////////////////////////////////////*/

vParticle::vParticle()
{
    weight = 1.0;
    likelihood = 1.0;
    minlikelihood = 20.0;
    inlierParameter = 1.5;
    outlierParameter = 3.0;
    stamp = 0;
    nextUpdate = 0;
    fixedrate = 0;
    tw = 0;
    inlierCount = 0;
    maxtw = 0;
    outlierCount = 0;
    angbuckets = 128;
    angdist.resize(angbuckets);
    negdist.resize(angbuckets);
}

void vParticle::initState(double x, double y, double r, double tw)
{
    this->x = x;
    this->y = y;
    this->r = r;
    this->tw = tw;

}


void vParticle::initState(double x, double y, double r, double vx, double vy,
              double vr)
{
    this->x = x;
    this->y = y;
    this->r = r;
}

void vParticle::initTiming(unsigned long int stamp)
{
    this->stamp = stamp;

    if(fixedrate) {
        this->nextUpdate = stamp + fixedrate;
    } else {
        this->nextUpdate = stamp + tw;
    }
}

unsigned int vParticle::getTemporalWindow()
{
    return tw;
}

void vParticle::initWeight(double weight)
{
    this->weight = weight;
}

bool vParticle::predict(unsigned long timestamp)
{
    double dt = timestamp - this->stamp;

    //double sigmatw = 20000;
    //tw += abs(generateGaussianNoise(0, sigmatw));
    //tw += timestamp - this->stamp;
    //tw *= 1.5;
    tw += (dt*1.2);

    double sigmap = 2.0;
    x = generateGaussianNoise(x, sigmap);
    y = generateGaussianNoise(y, sigmap);
    r = generateGaussianNoise(r, sigmap * 0.2);
    if(r < 8) r = 8;
    if(r > 44) r = 44;

    initTiming(timestamp);

    if(x < -r || y < -r || x > 304+r || y > 240+r)
        return true;
    return false;

}

bool vParticle::needsUpdating(unsigned long int stamp)
{
    return stamp > nextUpdate;
}



double vParticle::calcLikelihood(ev::vQueue &events, int nparticles)
{
    double defaultweight = 0.0001;
    if(events.empty()) {
        likelihood = 0.0001;
        return 0.0001;
    }

    double inliers = 0;
    double count = 0;
    double left = 0;
    double bottom = 0;
    for(unsigned int i = 0; i < events.size(); i++) {

        event<AddressEvent> v = std::static_pointer_cast<AddressEvent>(events[i]);

        double sqrd = sqrt(pow(v->getX() - x, 2.0) + pow(v->getY() - y, 2.0)) - r;
        sqrd = abs(sqrd);
        if(sqrd < 2) {
            inliers++; count++;
            left += v->getX() - x;
            bottom += v->getY() - y;
        } else if(sqrd < 4) {
            count -= 2;
        }
    }

    if(inliers) {
        double d_inliers = sqrt(pow(left / inliers, 2.0) + pow(bottom / inliers, 2.0));
        count -= d_inliers;
    }

    if(count < 0) count = 0;
    count += defaultweight;

    likelihood = count;


    return likelihood;
}

void vParticle::initLikelihood()
{
    likelihood = minlikelihood;
    inlierCount = 0;
    outlierCount = 0;
    angdist.zero();
    negdist.zero();
    maxtw = 0;
}

double approxatan2(double y, double x) {

    double ax = std::abs(x); double ay = std::abs(y);
    double a = std::min (ax, ay) / std::max (ax, ay);
    //double s = pow(a, 2.0);
    //double r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;

    double r = a * (M_PI_4 - (a - 1) * 0.273318560862312);

    if(ay > ax)
        r = 1.57079637 - r;

    if(x < 0)
        r = 3.14159274 - r;
    if(y < 0)
        r = -r;

    return r;

}

void vParticle::incrementalLikelihood(int vx, int vy, int dt)
{
    double dx = vx - x;
    double dy = vy - y;
    double sqrd = sqrt(pow(dx, 2.0) + pow(dy, 2.0)) - r;
    if(sqrd > -inlierParameter && sqrd < inlierParameter) {

        int a = 0.5 + (angbuckets-1) * (atan2(dy, dx) + M_PI) / (2.0 * M_PI);
        if(!angdist[a]) {
            inlierCount++;
            angdist[a] = 1;
        }

        double score = inlierCount - outlierCount;
        if(score > likelihood) {
            likelihood = score;
            maxtw = dt;

        }


    } else if(sqrd > -outlierParameter && sqrd < 0) { //-3 < X < -5

        int a = 0.5 + (angbuckets-1) * (atan2(dy, dx) + M_PI) / (2.0 * M_PI);
        if(!negdist[a]) {
            outlierCount++;
            negdist[a] = 1;
        }

    }

}

void vParticle::concludeLikelihood()
{
    if(likelihood > minlikelihood) tw = maxtw;
    weight = likelihood * weight;
}

void vParticle::updateWeight(double l, double n)
{
    weight = l * weight / n;
}

void vParticle::updateWeight2(double likelihood, double pwsumsq)
{

    weight = likelihood / (likelihood + pwsumsq/weight - weight);

}
void vParticle::updateWeightSync(double normval)
{
    weight = weight / normval;
}

void vParticle::resample(double w, unsigned long int t)
{
    initState(rand()%128, rand()%128, 10+rand()%26, 100);
    initWeight(w);
    initTiming(t);
}

void vParticle::resample(const vParticle &seeder, double w, unsigned long int t)
{

    x = seeder.x;
    y = seeder.y;
    r = seeder.r;

    stamp = seeder.stamp;
    weight = seeder.weight;

}
