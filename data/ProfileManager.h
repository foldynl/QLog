#ifndef QLOG_DATA_PROFILEMANAGER_H
#define QLOG_DATA_PROFILEMANAGER_H

#include <QString>
#include <QVariant>
#include <QMutex>
#include <QSettings>
#include <QLoggingCategory>
#include "core/debug.h"
#include <QtSql>

#define MOD_NAME "qlog.data.profilemanager"


/* the header file contains function implementation because
 * https://stackoverflow.com/questions/8752837/undefined-reference-to-template-class-constructor
 */


/* Question:
 * when I build, the compiler throws errors in every instance of the template class:
 *
 * undefined reference to `cola(float)::cola()'... (it's actually cola'<'float'>'::cola(),
 * but this doesn't let me use it like that.)
 */

/*
 * This is a common question in C++ programming. There are two valid answers to this. There are advantages
 * and disadvantages to both answers and your choice will depend on context. The common answer is to put all
 * the implementation in the header file, but there's another approach will will be suitable in some cases.
 * The choice is yours.
 *
 * The code in a template is merely a 'pattern' known to the compiler. The compiler won't compile the
 * constructors cola<float>::cola(...) and cola<string>::cola(...) until it is forced to do so.
 * And we must ensure that this compilation happens for the constructors at least once in the entire
 * compilation process, or we will get the 'undefined reference' error. (This applies to the other
 * methods of cola<T> also.)
 *
 * Understanding the problem:
 *
 * The problem is caused by the fact that main.cpp and cola.cpp will be compiled separately first.
 * In main.cpp, the compiler will implicitly instantiate the template classes cola<float> and
 * cola<string> because those particular instantiations are used in main.cpp. The bad news is that
 * the implementations of those member functions are not in main.cpp, nor in any header file included
 * in main.cpp, and therefore the compiler can't include complete versions of those functions in main.o.
 * When compiling cola.cpp, the compiler won't compile those instantiations either, because there are no
 * implicit or explicit instantiations of cola<float> or cola<string>. Remember, when compiling cola.cpp,
 * the compiler has no clue which instantiations will be needed; and we can't expect it to compile for
 * every type in order to ensure this problem never happens! (cola<int>, cola<char>, cola<ostream>,
 * cola< cola<int> > ... and so on ...)
 *
 * The two answers are:
 *     Tell the compiler, at the end of cola.cpp, which particular template classes will be required,
 *     forcing it to compile cola<float> and cola<string>.
 *     Put the implementation of the member functions in a header file that will be included every
 *     time any other 'translation unit' (such as main.cpp) uses the template class.
 *
 *  Answer 1: Explicitly instantiate the template, and its member definitions
 *  At the end of cola.cpp, you should add lines explicitly instantiating all the relevant templates, such as
 *
 *     template class cola<float>;
 *     template class cola<string>;
 *
 *  and you add the following two lines at the end of nodo_colaypila.cpp:
 *
 *     template class nodo_colaypila<float>;
 *     template class nodo_colaypila<std :: string>;
 *
 *  This will ensure that, when the compiler is compiling cola.cpp that it will explicitly compile
 *  all the code for the cola<float> and cola<string> classes. Similarly, nodo_colaypila.cpp
 *  contains the implementations of the nodo_colaypila<...> classes.
 *
 *  In this approach, you should ensure that all the of the implementation is placed into
 *  one .cpp file (i.e. one translation unit) and that the explicit instantation is placed
 *  after the definition of all the functions (i.e. at the end of the file).
 *
 *  Answer 2: Copy the code into the relevant header file
 *  The common answer is to move all the code from the implementation files cola.cpp and
 *  nodo_colaypila.cpp into cola.h and nodo_colaypila.h. In the long run, this is more
 *  flexible as it means you can use extra instantiations (e.g. cola<char>) without any
 *  more work. But it could mean the same functions are compiled many times, once in
 *  each translation unit. This is not a big problem, as the linker will correctly ignore
 *  the duplicate implementations. But it might slow down the compilation a little.
 *
 *  Summary
 *  The default answer, used by the STL for example and in most of the code that any
 *  of us will write, is to put all the implementations in the header files. But in a more
 *  private project, you will have more knowledge and control of which particular template
 *  classes will be instantiated. In fact, this 'bug' might be seen as a feature, as it
 *  stops users of your code from accidentally using instantiations you have not tested
 *  for or planned for ("I know this works for cola<float> and cola<string>, if you want
 *  to use something else, tell me first and will can verify it works before enabling it.").
*/
template<class T>
class ProfileManagerSQL
{
public:
    explicit ProfileManagerSQL(const QString &tableName)
        : tableName(tableName)
    {
        QString logging_cat(MOD_NAME); logging_cat.append(".function.entered");
        qCDebug(QLoggingCategory(logging_cat.toLatin1().constData()));

        QSqlQuery query(QString("SELECT profile_name FROM %1 WHERE IFNULL(selected, 0) = 1").arg(tableName));
        currentProfile1 = query.first() ? query.value(0).toString() : QString();

        if ( currentProfile1.isEmpty() )
            qDebug() << "Empty profile name for " << tableName
                     << "SQL Error" << query.lastError().text();
    };

    const T getCurProfile1()
    {
        QString logging_cat(MOD_NAME); logging_cat.append(".function.entered");
        qCDebug(QLoggingCategory(logging_cat.toLatin1().constData()));

        return ( ! currentProfile1.isEmpty() ) ? getProfile(currentProfile1)
                                               : T();
    };

    void __setCurProfile1(const QString &profileName)
    {
        if ( profiles.contains(profileName) || profileName.isEmpty() )
        {
            QSqlQuery query;

            // atomic change
            if ( !query.prepare(QString("UPDATE %1 "
                                              "SET selected = CASE "
                                              "               WHEN profile_name = :profileName THEN 1 "
                                              "               ELSE NULL "
                                              "               END "
                                              "WHERE selected = 1 OR profile_name = :profileName2").arg(tableName)) )
            {
                qWarning() << "Cannot prepare Update statement for" << tableName;
                return;
            }

            query.bindValue(":profileName", profileName);
            query.bindValue(":profileName2", profileName);

            if ( query.exec() )
                currentProfile1 = profileName;
            else
                qWarning() << "Cannot set the selected profile for " << tableName
                           << query.lastError().text();
        }
        else
            qWarning() << "Cannot set Current Profile to "
                       << profileName
                       << "because is not not a valid profile name" << tableName;
    };

    void setCurProfile1(const QString &profileName)
    {
        QString logging_cat(MOD_NAME); logging_cat.append(".function.entered");
        qCDebug(QLoggingCategory(logging_cat.toLatin1().constData()));

        currProfMutex.lock();
        __setCurProfile1(profileName);
        currProfMutex.unlock();
    };

    void saveCurProfile1()
    {
        QString logging_cat(MOD_NAME); logging_cat.append(".function.entered");
        qCDebug(QLoggingCategory(logging_cat.toLatin1().constData()));

        currProfMutex.lock();
        __setCurProfile1(currentProfile1);
        currProfMutex.unlock();
    };

    const T getProfile(const QString &profileName)
    {
        QString logging_cat(MOD_NAME); logging_cat.append(".function.entered");
        qCDebug(QLoggingCategory(logging_cat.toLatin1().constData()));

        if ( profiles.contains(profileName) )
        {
            profilesMutex.lock();
            T ret = profiles.value(profileName).template value<T>();
            profilesMutex.unlock();
            return ret;
        }
        else
        {
            if ( !profileName.isEmpty() )
                qWarning() << "Profile " << profileName << " not found" << tableName;
            return T();
        }
    };

    void addProfile(const QString &profileName, T profile)
    {
        QString logging_cat(MOD_NAME); logging_cat.append(".function.entered");
        qCDebug(QLoggingCategory(logging_cat.toLatin1().constData()));

        profilesMutex.lock();
        profiles.insert(profileName, QVariant::fromValue(profile));
        profilesMutex.unlock();
    };

    int removeProfile(const QString &profileName)
    {
        QString logging_cat(MOD_NAME); logging_cat.append(".function.entered");
        qCDebug(QLoggingCategory(logging_cat.toLatin1().constData()));

        currProfMutex.lock();
        if ( currentProfile1 == profileName )
        {
            __setCurProfile1(QString());
        }
        currProfMutex.unlock();

        profilesMutex.lock();
        int ret = profiles.remove(profileName);
        profilesMutex.unlock();

        return ret;
    };

    const QStringList profileNameList()
    {
        QString logging_cat(MOD_NAME); logging_cat.append(".function.entered");
        qCDebug(QLoggingCategory(logging_cat.toLatin1().constData()));

        profilesMutex.lock();
        QStringList ret(profiles.keys());
        profilesMutex.unlock();

        return ret;
    };

private:
    QMap<QString, QVariant> profiles;
    QString currentProfile1;
    QString tableName;
    QMutex profilesMutex;
    QMutex currProfMutex;
};

#endif // QLOG_DATA_PROFILEMANAGER_H
