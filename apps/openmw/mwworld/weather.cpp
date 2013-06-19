#include "weather.hpp"

#include <ctime>
#include <cstdlib>

#include <boost/algorithm/string.hpp>
#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/soundmanager.hpp"

#include "../mwrender/renderingmanager.hpp"

#include "player.hpp"
#include "esmstore.hpp"
#include "fallback.hpp"

using namespace Ogre;
using namespace MWWorld;
using namespace MWSound;

namespace
{
    float lerp (float x, float y, float factor)
    {
        return x * (1-factor) + y * factor;
    }

    Ogre::ColourValue lerp (const Ogre::ColourValue& x, const Ogre::ColourValue& y, float factor)
    {
        return x * (1-factor) + y * factor;
    }
}

std::string Weather::weatherTypeToStr(Weather::Type type)
{
    switch (type) {
        case Type_Clear:
            return "Clear";
        case Type_Cloudy:
            return "Cloudy";
        case Type_Foggy:
            return "Foggy";
        case Type_Overcast:
            return "Overcast";
        case Type_Rain:
            return "Rain";
        case Type_Thunderstorm:
            return "Thunderstorm";
        case Type_Ashstorm:
            return "Ashstorm";
        case Type_Blight:
            return "Blight";
        case Type_Snow:
            return "Snow";
        case Type_Blizzard:
            return "Blizzard";
        default: // Type_Unknown
            return "";
    }
}

void WeatherManager::setFallbackWeather(Weather& weather, Weather::Type type)
{
    const std::string weatherName = Weather::weatherTypeToStr(type);
    weather.mCloudsMaximumPercent = mFallback->getFallbackFloat("Weather_"+weatherName+"_Clouds_Maximum_Percent");
    weather.mTransitionDelta = mFallback->getFallbackFloat("Weather_"+weatherName+"_Transition_Delta");
    weather.mSkySunriseColor= mFallback->getFallbackColour("Weather_"+weatherName+"_Sky_Sunrise_Color");
    weather.mSkyDayColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Sky_Day_Color");
    weather.mSkySunsetColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Sky_Sunset_Color");
    weather.mSkyNightColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Sky_Night_Color");
    weather.mFogSunriseColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Fog_Sunrise_Color");
    weather.mFogDayColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Fog_Day_Color");
    weather.mFogSunsetColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Fog_Sunset_Color");
    weather.mFogNightColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Fog_Night_Color");
    weather.mAmbientSunriseColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Ambient_Sunrise_Color");
    weather.mAmbientDayColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Ambient_Day_Color");
    weather.mAmbientSunsetColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Ambient_Sunset_Color");
    weather.mAmbientNightColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Ambient_Night_Color");
    weather.mSunSunriseColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Sun_Sunrise_Color");
    weather.mSunDayColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Sun_Day_Color");
    weather.mSunSunsetColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Sun_Sunset_Color");
    weather.mSunNightColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Sun_Night_Color");
    weather.mSunDiscSunsetColor = mFallback->getFallbackColour("Weather_"+weatherName+"_Sun_Disc_Sunset_Color");
    weather.mLandFogDayDepth = mFallback->getFallbackFloat("Weather_"+weatherName+"_Land_Fog_Day_Depth");
    weather.mLandFogNightDepth = mFallback->getFallbackFloat("Weather_"+weatherName+"_Land_Fog_Night_Depth");
    weather.mWindSpeed = mFallback->getFallbackFloat("Weather_"+weatherName+"_Wind_Speed");
    weather.mCloudSpeed = mFallback->getFallbackFloat("Weather_"+weatherName+"_Cloud_Speed");
    weather.mGlareView = mFallback->getFallbackFloat("Weather_"+weatherName+"_Glare_View");
    mWeatherSettings[type] = weather;
}


float WeatherManager::calculateHourFade (const std::string& moonName) const
{
    float fadeOutStart=mFallback->getFallbackFloat("Moons_"+moonName+"_Fade_Out_Start");
    float fadeOutFinish=mFallback->getFallbackFloat("Moons_"+moonName+"_Fade_Out_Finish");
    float fadeInStart=mFallback->getFallbackFloat("Moons_"+moonName+"_Fade_In_Start");
    float fadeInFinish=mFallback->getFallbackFloat("Moons_"+moonName+"_Fade_In_Finish");

    if (mHour >= fadeOutStart && mHour <= fadeOutFinish)
        return (1 - ((mHour - fadeOutStart) / (fadeOutFinish - fadeOutStart)));
    if (mHour >= fadeInStart && mHour <= fadeInFinish)
        return (1 - ((mHour - fadeInStart) / (fadeInFinish - fadeInStart)));
    else
        return 1;
}

float WeatherManager::calculateAngleFade (const std::string& moonName, float angle) const
{
    float endAngle=mFallback->getFallbackFloat("Moons_"+moonName+"_Fade_End_Angle");
    float startAngle=mFallback->getFallbackFloat("Moons_"+moonName+"_Fade_Start_Angle");
    if (angle <= startAngle && angle >= endAngle)
        return (1 - ((angle - endAngle)/(startAngle-endAngle)));
    else if (angle > startAngle)
        return 0.f;
    else
        return 1.f;
}

WeatherManager::WeatherManager(MWRender::RenderingManager* rendering,MWWorld::Fallback* fallback) :
     mHour(14), mCurrentWeather(Weather::Type_Clear), mFirstUpdate(true), mWeatherUpdateTime(0),
     mThunderFlash(0), mThunderChance(0), mThunderChanceNeeded(50), mThunderSoundDelay(0),
     mRemainingTransitionTime(0), mMonth(0), mDay(0),
     mTimePassed(0), mFallback(fallback), mWindSpeed(0.f), mRendering(rendering)
{
    //Globals
    mThunderSoundID0 = mFallback->getFallbackString("Weather_Thunderstorm_Thunder_Sound_ID_0");
    mThunderSoundID1 = mFallback->getFallbackString("Weather_Thunderstorm_Thunder_Sound_ID_1");
    mThunderSoundID2 = mFallback->getFallbackString("Weather_Thunderstorm_Thunder_Sound_ID_2");
    mThunderSoundID3 = mFallback->getFallbackString("Weather_Thunderstorm_Thunder_Sound_ID_3");
    mSunriseTime = mFallback->getFallbackFloat("Weather_Sunrise_Time");
    mSunsetTime = mFallback->getFallbackFloat("Weather_Sunset_Time");
    mSunriseDuration = mFallback->getFallbackFloat("Weather_Sunrise_Duration");
    mSunsetDuration = mFallback->getFallbackFloat("Weather_Sunset_Duration");
    mHoursBetweenWeatherChanges = mFallback->getFallbackFloat("Weather_Hours_Between_Weather_Changes");
    mWeatherUpdateTime = mHoursBetweenWeatherChanges * 3600;
    mThunderFrequency = mFallback->getFallbackFloat("Weather_Thunderstorm_Thunder_Frequency");
    mThunderThreshold = mFallback->getFallbackFloat("Weather_Thunderstorm_Thunder_Threshold");
    mThunderSoundDelay = 0.25;

    //Some useful values
    /* TODO: Use pre-sunrise_time, pre-sunset_time,
     * post-sunrise_time, and post-sunset_time to better
     * describe sunrise/sunset time.
     * These values are fallbacks attached to weather.
     */
    mNightStart = mSunsetTime + mSunsetDuration;
    mNightEnd = mSunriseTime - 0.5;
    mDayStart = mSunriseTime + mSunriseDuration;
    mDayEnd = mSunsetTime;

    //Weather
    Weather clear;
    clear.mCloudTexture = "tx_sky_clear.dds";
    setFallbackWeather(clear, Weather::Type_Clear);

    Weather cloudy;
    cloudy.mCloudTexture = "tx_sky_cloudy.dds";
    setFallbackWeather(cloudy, Weather::Type_Cloudy);

    Weather foggy;
    foggy.mCloudTexture = "tx_sky_foggy.dds";
    setFallbackWeather(foggy, Weather::Type_Foggy);

    Weather thunderstorm;
    thunderstorm.mCloudTexture = "tx_sky_thunder.dds";
    thunderstorm.mRainLoopSoundID = "rain heavy";
    setFallbackWeather(thunderstorm, Weather::Type_Thunderstorm);

    Weather rain;
    rain.mCloudTexture = "tx_sky_rainy.dds";
    rain.mRainLoopSoundID = "rain";
    setFallbackWeather(rain, Weather::Type_Rain);

    Weather overcast;
    overcast.mCloudTexture = "tx_sky_overcast.dds";
    setFallbackWeather(overcast, Weather::Type_Overcast);

    Weather ashstorm;
    ashstorm.mCloudTexture = "tx_sky_ashstorm.dds";
    ashstorm.mAmbientLoopSoundID = "ashstorm";
    setFallbackWeather(ashstorm, Weather::Type_Ashstorm);

    Weather blight;
    blight.mCloudTexture = "tx_sky_blight.dds";
    blight.mAmbientLoopSoundID = "blight";
    setFallbackWeather(blight, Weather::Type_Blight);

    Weather snow;
    snow.mCloudTexture = "tx_bm_sky_snow.dds";
    setFallbackWeather(snow, Weather::Type_Snow);

    Weather blizzard;
    blizzard.mCloudTexture = "tx_bm_sky_blizzard.dds";
    blizzard.mAmbientLoopSoundID = "BM Blizzard";
    setFallbackWeather(blizzard, Weather::Type_Blizzard);
}

void WeatherManager::setWeather(Weather::Type weatherType, bool instant)
{
    if (weatherType == mCurrentWeather && mNextWeather == Weather::Type_Unknown)
    {
        mFirstUpdate = false;
        return;
    }

    if (instant || mFirstUpdate)
    {
        mNextWeather = Weather::Type_Unknown;
        mCurrentWeather = weatherType;
    }
    else
    {
        if (mNextWeather != Weather::Type_Unknown)
        {
            // transition more than 50% finished?
            if (mRemainingTransitionTime/(mWeatherSettings[mCurrentWeather].mTransitionDelta * 24.f * 3600) <= 0.5)
                mCurrentWeather = mNextWeather;
        }

        mNextWeather = weatherType;
        mRemainingTransitionTime = mWeatherSettings[mCurrentWeather].mTransitionDelta * 24.f * 3600;
    }
    mFirstUpdate = false;
}

WeatherResult WeatherManager::getResult(Weather::Type weatherType)
{
    const Weather& current = mWeatherSettings[weatherType];
    WeatherResult result;

    result.mCloudTexture = current.mCloudTexture;
    result.mCloudBlendFactor = 0;
    result.mCloudOpacity = current.mCloudsMaximumPercent;
    result.mWindSpeed = current.mWindSpeed;
    result.mCloudSpeed = current.mCloudSpeed;
    result.mGlareView = current.mGlareView;
    result.mAmbientLoopSoundID = current.mAmbientLoopSoundID;
    result.mSunColor = current.mSunDiscSunsetColor;
 
    result.mNight = (mHour < mSunriseTime || mHour > mNightStart - 1);

    result.mFogDepth = result.mNight ? current.mLandFogNightDepth : current.mLandFogDayDepth;

    // night
    if (mHour <= mNightEnd || mHour >= mNightStart + 1)
    {
        result.mFogColor = current.mFogNightColor;
        result.mAmbientColor = current.mAmbientNightColor;
        result.mSunColor = current.mSunNightColor;
        result.mSkyColor = current.mSkyNightColor;
        result.mNightFade = 1.f;
    }

    // sunrise
    else if (mHour >= mNightEnd && mHour <= mDayStart + 1)
    {
        if (mHour <= mSunriseTime)
        {
            // fade in
            float advance = mSunriseTime - mHour;
            float factor = advance / 0.5f;
            result.mFogColor = lerp(current.mFogSunriseColor, current.mFogNightColor, factor);
            result.mAmbientColor = lerp(current.mAmbientSunriseColor, current.mAmbientNightColor, factor);
            result.mSunColor = lerp(current.mSunSunriseColor, current.mSunNightColor, factor);
            result.mSkyColor = lerp(current.mSkySunriseColor, current.mSkyNightColor, factor);
            result.mNightFade = factor;
        }
        else //if (mHour >= 6)
        {
            // fade out
            float advance = mHour - mSunriseTime;
            float factor = advance / 3.f;
            result.mFogColor = lerp(current.mFogSunriseColor, current.mFogDayColor, factor);
            result.mAmbientColor = lerp(current.mAmbientSunriseColor, current.mAmbientDayColor, factor);
            result.mSunColor = lerp(current.mSunSunriseColor, current.mSunDayColor, factor);
            result.mSkyColor = lerp(current.mSkySunriseColor, current.mSkyDayColor, factor);
        }
    }

    // day
    else if (mHour >= mDayStart + 1 && mHour <= mDayEnd - 1)
    {
        result.mFogColor = current.mFogDayColor;
        result.mAmbientColor = current.mAmbientDayColor;
        result.mSunColor = current.mSunDayColor;
        result.mSkyColor = current.mSkyDayColor;
    }

    // sunset
    else if (mHour >= mDayEnd - 1 && mHour <= mNightStart + 1)
    {
        if (mHour <= mDayEnd + 1)
        {
            // fade in
            float advance = (mDayEnd + 1) - mHour;
            float factor = (advance / 2);
            result.mFogColor = lerp(current.mFogSunsetColor, current.mFogDayColor, factor);
            result.mAmbientColor = lerp(current.mAmbientSunsetColor, current.mAmbientDayColor, factor);
            result.mSunColor = lerp(current.mSunSunsetColor, current.mSunDayColor, factor);
            result.mSkyColor = lerp(current.mSkySunsetColor, current.mSkyDayColor, factor);
        }
        else //if (mHour >= 19)
        {
            // fade out
            float advance = mHour - (mDayEnd + 1);
            float factor = advance / 2.f;
            result.mFogColor = lerp(current.mFogSunsetColor, current.mFogNightColor, factor);
            result.mAmbientColor = lerp(current.mAmbientSunsetColor, current.mAmbientNightColor, factor);
            result.mSunColor = lerp(current.mSunSunsetColor, current.mSunNightColor, factor);
            result.mSkyColor = lerp(current.mSkySunsetColor, current.mSkyNightColor, factor);
            result.mNightFade = factor;
        }
    }

    return result;
}

WeatherResult WeatherManager::transition(float factor)
{
    const WeatherResult& current = getResult(mCurrentWeather);
    const WeatherResult& other = getResult(mNextWeather);
    WeatherResult result;

    result.mCloudTexture = current.mCloudTexture;
    result.mNextCloudTexture = other.mCloudTexture;
    result.mCloudBlendFactor = factor;

    result.mCloudOpacity = lerp(current.mCloudOpacity, other.mCloudOpacity, factor);
    result.mFogColor = lerp(current.mFogColor, other.mFogColor, factor);
    result.mSunColor = lerp(current.mSunColor, other.mSunColor, factor);
    result.mSkyColor = lerp(current.mSkyColor, other.mSkyColor, factor);

    result.mAmbientColor = lerp(current.mAmbientColor, other.mAmbientColor, factor);
    result.mSunDiscColor = lerp(current.mSunDiscColor, other.mSunDiscColor, factor);
    result.mFogDepth = lerp(current.mFogDepth, other.mFogDepth, factor);
    result.mWindSpeed = lerp(current.mWindSpeed, other.mWindSpeed, factor);
    result.mCloudSpeed = lerp(current.mCloudSpeed, other.mCloudSpeed, factor);
    result.mCloudOpacity = lerp(current.mCloudOpacity, other.mCloudOpacity, factor);
    result.mGlareView = lerp(current.mGlareView, other.mGlareView, factor);
    result.mNightFade = lerp(current.mNightFade, other.mNightFade, factor);

    result.mNight = current.mNight;

    return result;
}

void WeatherManager::update(float duration)
{
    float timePassed = mTimePassed;
    mTimePassed = 0;

    mWeatherUpdateTime -= timePassed;

    bool exterior = (MWBase::Environment::get().getWorld()->isCellExterior() || MWBase::Environment::get().getWorld()->isCellQuasiExterior());

    if (exterior)
    {
        std::string regionstr = Misc::StringUtils::lowerCase(MWBase::Environment::get().getWorld()->getPlayer().getPlayer().getCell()->mCell->mRegion);

        if (mWeatherUpdateTime <= 0 || regionstr != mCurrentRegion)
        {
            mCurrentRegion = regionstr;
            mWeatherUpdateTime = mHoursBetweenWeatherChanges * 3600;

            Weather::Type weatherType = Weather::Type_Clear;

            if (mRegionOverrides.find(regionstr) != mRegionOverrides.end())
                weatherType = mRegionOverrides[regionstr];
            else
            {
                // get weather probabilities for the current region
                const ESM::Region *region =
                    MWBase::Environment::get().getWorld()->getStore().get<ESM::Region>().search (regionstr);

                if (region != 0)
                {
                    /*
                     * All probabilities must add to 100 (responsibility of the user).
                     * If chances A and B has values 30 and 70 then by generating
                     * 100 numbers 1..100, 30% will be lesser or equal 30 and
                     * 70% will be greater than 30 (in theory).
                     */
                    const int probability[] = {
                        region->mData.mClear,
                        region->mData.mCloudy,
                        region->mData.mFoggy,
                        region->mData.mOvercast,
                        region->mData.mRain,
                        region->mData.mThunder,
                        region->mData.mAsh,
                        region->mData.mBlight,
                        region->mData.mA,
                        region->mData.mB
                    }; // 10 elements

                    int chance = (rand() % 100) + 1; // 1..100
                    int sum = 0;
                    for (int i = 0; i < 10; ++i)
                    {
                        sum += probability[i];
                        if (chance < sum)
                        {
                            weatherType = (Weather::Type)i;
                            break;
                        }
                    }
                }
            }

            setWeather(weatherType, false);
        }

        WeatherResult result;

        if (mNextWeather != Weather::Type_Unknown)
        {
            mRemainingTransitionTime -= timePassed;
            if (mRemainingTransitionTime < 0)
            {
                mCurrentWeather = mNextWeather;
                mNextWeather = Weather::Type_Unknown;
            }
        }

        if (mNextWeather != Weather::Type_Unknown)
            result = transition(1 - (mRemainingTransitionTime / (mWeatherSettings[mCurrentWeather].mTransitionDelta * 24.f * 3600)));
        else
            result = getResult(mCurrentWeather);

        mWindSpeed = result.mWindSpeed;

        mRendering->configureFog(result.mFogDepth, result.mFogColor);

        // disable sun during night
        if (mHour >= mNightStart || mHour <= mSunriseTime)
            mRendering->getSkyManager()->sunDisable();
        else
            mRendering->getSkyManager()->sunEnable();

        // sun angle
        float height;

        //Day duration
        float dayDuration = (mNightStart - 1) - mSunriseTime;

        // rise at 6, set at 20
        if (mHour >= mSunriseTime && mHour <= mNightStart)
            height = 1 - std::abs(((mHour - dayDuration) / 7.f));
        else if (mHour > mNightStart)
            height = (mHour - mNightStart) / 4.f;
        else //if (mHour > 0 && mHour < 6)
            height = 1 - (mHour / mSunriseTime);

        int facing = (mHour > 13.f) ? 1 : -1;

        Vector3 final(
            (height - 1) * facing,
            (height - 1) * facing,
            height);
        mRendering->setSunDirection(final);

        /*
         * TODO: import separated fadeInStart/Finish, fadeOutStart/Finish
         * for masser and secunda
         */

        float fadeOutFinish=mFallback->getFallbackFloat("Moons_Masser_Fade_Out_Finish");
        float fadeInStart=mFallback->getFallbackFloat("Moons_Masser_Fade_In_Start");

        //moon calculations
        float moonHeight;
        if (mHour >= fadeInStart)
            moonHeight = mHour - fadeInStart;
        else if (mHour <= fadeOutFinish)
            moonHeight = mHour + fadeOutFinish;
        else
            moonHeight = 0;

        moonHeight /= (24.f - (fadeInStart - fadeOutFinish));

        if (moonHeight != 0)
        {
            int facing = (moonHeight <= 1) ? 1 : -1;
            Vector3 masser(
                (moonHeight - 1) * facing,
                (1 - moonHeight) * facing,
                moonHeight);

            Vector3 secunda(
                (moonHeight - 1) * facing * 1.25,
                (1 - moonHeight) * facing * 0.8,
                moonHeight);

            mRendering->getSkyManager()->setMasserDirection(masser);
            mRendering->getSkyManager()->setSecundaDirection(secunda);
            mRendering->getSkyManager()->masserEnable();
            mRendering->getSkyManager()->secundaEnable();

            float angle = (1-moonHeight) * 90.f * facing;
            float masserHourFade = calculateHourFade("Masser");
            float secundaHourFade = calculateHourFade("Secunda");
            float masserAngleFade = calculateAngleFade("Masser", angle);
            float secundaAngleFade = calculateAngleFade("Secunda", angle);

            masserAngleFade *= masserHourFade;
            secundaAngleFade *= secundaHourFade;

            mRendering->getSkyManager()->setMasserFade(masserAngleFade);
            mRendering->getSkyManager()->setSecundaFade(secundaAngleFade);
        }
        else
        {
            mRendering->getSkyManager()->masserDisable();
            mRendering->getSkyManager()->secundaDisable();
        }

        if (mCurrentWeather == Weather::Type_Thunderstorm && mNextWeather == Weather::Type_Unknown && exterior)
        {
            if (mThunderFlash > 0)
            {
                // play the sound after a delay
                mThunderSoundDelay -= duration;
                if (mThunderSoundDelay <= 0)
                {
                    // pick a random sound
                    int sound = rand() % 4;
                    std::string soundname;
                    if (sound == 0) soundname = mThunderSoundID0;
                    else if (sound == 1) soundname = mThunderSoundID1;
                    else if (sound == 2) soundname = mThunderSoundID2;
                    else if (sound == 3) soundname = mThunderSoundID3;
                    MWBase::Environment::get().getSoundManager()->playSound(soundname, 1.0, 1.0);
                    mThunderSoundDelay = 1000;
                }

                mThunderFlash -= duration;
                if (mThunderFlash > 0)
                    mRendering->getSkyManager()->setLightningStrength( mThunderFlash / mThunderThreshold );
                else
                {
                    srand(time(NULL));
                    mThunderChanceNeeded = rand() % 100;
                    mThunderChance = 0;
                    mRendering->getSkyManager()->setLightningStrength( 0.f );
                }
            }
            else
            {
                // no thunder active
                mThunderChance += duration*4; // chance increases by 4 percent every second
                if (mThunderChance >= mThunderChanceNeeded)
                {
                    mThunderFlash = mThunderThreshold;

                    mRendering->getSkyManager()->setLightningStrength( mThunderFlash / mThunderThreshold );

                    mThunderSoundDelay = 0.25;
                }
            }
        }
        else
            mRendering->getSkyManager()->setLightningStrength(0.f);

        mRendering->setAmbientColour(result.mAmbientColor);
        mRendering->sunEnable(false);
        mRendering->setSunColour(result.mSunColor);

        mRendering->getSkyManager()->setWeather(result);
    }
    else
    {
        mRendering->sunDisable(false);
        mRendering->skyDisable();
        mRendering->getSkyManager()->setLightningStrength(0.f);
    }

    // play sounds
    std::string ambientSnd = (mNextWeather == Weather::Type_Unknown ? mWeatherSettings[mCurrentWeather].mAmbientLoopSoundID : "");
    if (!exterior) ambientSnd = "";
    if (ambientSnd != "")
    {
        if (std::find(mSoundsPlaying.begin(), mSoundsPlaying.end(), ambientSnd) == mSoundsPlaying.end())
        {
            mSoundsPlaying.push_back(ambientSnd);
            MWBase::Environment::get().getSoundManager()->playSound(ambientSnd, 1.0, 1.0, MWBase::SoundManager::Play_Loop);
        }
    }

    std::string rainSnd = (mNextWeather == Weather::Type_Unknown ? mWeatherSettings[mCurrentWeather].mRainLoopSoundID : "");
    if (!exterior) rainSnd = "";
    if (rainSnd != "")
    {
        if (std::find(mSoundsPlaying.begin(), mSoundsPlaying.end(), rainSnd) == mSoundsPlaying.end())
        {
            mSoundsPlaying.push_back(rainSnd);
            MWBase::Environment::get().getSoundManager()->playSound(rainSnd, 1.0, 1.0, MWBase::SoundManager::Play_Loop);
        }
    }

    // stop sounds
    std::vector<std::string>::iterator it=mSoundsPlaying.begin();
    while (it!=mSoundsPlaying.end())
    {
        if ( *it != ambientSnd && *it != rainSnd)
        {
            MWBase::Environment::get().getSoundManager()->stopSound(*it);
            it = mSoundsPlaying.erase(it);
        }
        else
            ++it;
    }
}

void WeatherManager::setHour(const float hour)
{
    mHour = hour;
}

void WeatherManager::setDate(const int day, const int month)
{
    mDay = day;
    mMonth = month;
}

unsigned int WeatherManager::getWeatherID() const
{
    // Source: http://www.uesp.net/wiki/Tes3Mod:GetCurrentWeather
    return mCurrentWeather;
}

void WeatherManager::changeWeather(const std::string& region, const int id)
{
    // make sure this region exists
    MWBase::Environment::get().getWorld()->getStore().get<ESM::Region>().find(region);

    Weather::Type weatherType = Weather::Type_Clear;
    if (id >= Weather::Type_Clear && id < Weather::Type_Unknown)
        weatherType = (Weather::Type)id;

    mRegionOverrides[Misc::StringUtils::lowerCase(region)] = weatherType;

    std::string playerRegion = MWBase::Environment::get().getWorld()->getPlayer().getPlayer().getCell()->mCell->mRegion;
    if (Misc::StringUtils::ciEqual(region, playerRegion))
        setWeather(weatherType);
}

float WeatherManager::getWindSpeed() const
{
    return mWindSpeed;
}
