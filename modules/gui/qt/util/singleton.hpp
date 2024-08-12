/*****************************************************************************
 * singleton.hpp: Generic Singleton pattern implementation
 ****************************************************************************
 * Copyright (C) 2009 VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_QT_SINGLETON_HPP_
#define VLC_QT_SINGLETON_HPP_

#include <type_traits>
#include <memory>

#include <QMutex>
#include <QMutexLocker>
#include <QJSEngine>
#include <QQmlEngine>

template <typename T>
class Singleton
{
public:
    template <bool create, class = typename std::enable_if<!create>::type>
    static T* getInstance( void )
    {
        QMutexLocker lock( &m_mutex );
        return m_instance;
    }

    template <class T2 = T, typename... Args>
    static T* getInstance( Args&&... args )
    {
        QMutexLocker lock( &m_mutex );
        if ( !m_instance )
          m_instance = new T2( std::forward<Args>( args )... );
        return m_instance;
    }

    static void killInstance()
    {
        QMutexLocker lock( &m_mutex );
        delete m_instance;
        m_instance = nullptr;
    }

protected:
    Singleton(){}
    virtual ~Singleton(){}
    /* Not implemented since these methods should *NEVER* been called.
    If they do, it probably won't compile :) */
    Singleton(const Singleton<T>&);
    Singleton<T>&   operator=(const Singleton<T>&);

private:
    inline static T* m_instance = nullptr;
    inline static QMutex m_mutex;
};

template<typename T>
class QMLSingleton
{
public:
    //method used by QmlEngine to retreive singletons
    static T* create(QQmlEngine *, QJSEngine *engine)
    {
        // The instance has to exist before it is used. We cannot replace it.
        assert(s_instance);

        // The engine has to have the same thread affinity as the singleton.
        assert(engine->thread() == s_instance->thread());

        // There can only be one engine accessing the singleton.
        if (s_engine)
            assert(engine == s_engine);
        else
            s_engine = engine;

        // Explicitly specify C++ ownership so that the engine doesn't delete
        // the instance.
        QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
        return s_instance;
    }

    template <class T2 = T, typename... Args>
    static T2* createInstance( Args&&... args )
    {
        assert( !s_instance );
        T2* obj = new T2(std::forward<Args>( args )... );
        s_instance = obj;
        return obj;
    }

    static T* getInstance( void )
    {
        return s_instance;
    }

    static void killInstance()
    {
        if (s_instance) {
            delete s_instance;
            s_instance = nullptr;
        }
        s_engine = nullptr;
    }

protected:
    QMLSingleton(){}
    virtual ~QMLSingleton(){}
    /* Not implemented since these methods should *NEVER* been called */
    QMLSingleton(const QMLSingleton<T>&) = delete;
    QMLSingleton<T>&   operator=(const QMLSingleton<T>&) = delete;

private:
    inline static T* s_instance = nullptr;
    inline static QJSEngine* s_engine = nullptr;
};

#endif // include-guard
