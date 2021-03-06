#include "stdafx.h"
#include "ResponseListener.h"
#include "SessionStatusListener.h"
#include "TableListener.h"
#include "LoginParams.h"
#include "SampleParams.h"
#include "CommonSources.h"

void printHelp(std::string &);
bool checkObligatoryParams(LoginParams *, SampleParams *);
void printSampleParams(std::string &, LoginParams *, SampleParams *);
IO2GOrderTableRow *getOrder(IO2GTableManager *tableManager, const char *sOrderID);
IO2GRequest* attachStopOrderRequest(IO2GSession * session, IO2GOrderTableRow* order, const char* accountID, bool isPegged, const char* pegType, double pegOffset, double rate);

int roundPrice(double x)
{
    return (int)(floor(x + 0.5));
}

int main(int argc, char *argv[])
{
    std::string procName = "AttachStopOrder";
    if (argc == 1)
    {
        printHelp(procName);
        return -1;
    }

    LoginParams *loginParams = new LoginParams(argc, argv);
    SampleParams *sampleParams = new SampleParams(argc, argv);

    printSampleParams(procName, loginParams, sampleParams);
    if (!checkObligatoryParams(loginParams, sampleParams))
    {
        delete loginParams;
        delete sampleParams;
        return -1;
    }

    IO2GSession *session = CO2GTransport::createSession();
    session->useTableManager(Yes, 0);

    SessionStatusListener *sessionListener = new SessionStatusListener(session, false,
                                                                       loginParams->getSessionID(),
                                                                       loginParams->getPin());
    session->subscribeSessionStatus(sessionListener);

    bool bConnected = login(session, sessionListener, loginParams);
    bool bWasError = false;

    if (bConnected)
    {
        bool bIsAccountEmpty = !sampleParams->getAccount() || strlen(sampleParams->getAccount()) == 0;
        ResponseListener *responseListener = new ResponseListener();
        TableListener *tableListener = new TableListener(responseListener);
        session->subscribeResponse(responseListener);

        O2G2Ptr<IO2GTableManager> tableManager = session->getTableManager();
        O2GTableManagerStatus managerStatus = tableManager->getStatus();
        while (managerStatus == TablesLoading)
        {
            Sleep(50);
            managerStatus = tableManager->getStatus();
        }

        if (managerStatus == TablesLoadFailed)
        {
            std::cout << "Cannot refresh all tables of table manager" << std::endl;
        }

        O2G2Ptr<IO2GAccountRow> account = getAccount(tableManager, sampleParams->getAccount());

        if (account)
        {
            if (bIsAccountEmpty)
            {
                sampleParams->setAccount(account->getAccountID());
                std::cout << "Account: " << sampleParams->getAccount() << std::endl;
            }
            O2G2Ptr<IO2GOrderTableRow> order = getOrder(tableManager, sampleParams->getOrderID());

            if (order)
            {
                O2G2Ptr<IO2GLoginRules> loginRules = session->getLoginRules();
                if (loginRules)
                {
                    O2G2Ptr<IO2GTradingSettingsProvider> tradingSettingsProvider = loginRules->getTradingSettingsProvider();
                    int iBaseUnitSize = tradingSettingsProvider->getBaseUnitSize(sampleParams->getInstrument(), account);
                    int iAmount = iBaseUnitSize * sampleParams->getLots();
                    int iCondDistEntryLimit = tradingSettingsProvider->getCondDistEntryLimit(sampleParams->getInstrument());
                    int iCondDistEntryStop = tradingSettingsProvider->getCondDistEntryStop(sampleParams->getInstrument());

                    O2G2Ptr<IO2GRequest> request = attachStopOrderRequest(session, order, account->getAccountID(), sampleParams->IsPegged(), sampleParams->getPegType(),
                        sampleParams->getPegOffset(), sampleParams->getRate());

                    if (request)
                    {
                        tableListener->subscribeEvents(tableManager);

                        responseListener->setRequestID(request->getRequestID());
                        tableListener->setRequestID(request->getRequestID());
                        session->sendRequest(request);
                        if (responseListener->waitEvents())
                        {
                            std::cout << "Done!" << std::endl;
                        }
                        else
                        {
                            std::cout << "Response waiting timeout expired" << std::endl;
                            bWasError = true;
                        }

                        tableListener->unsubscribeEvents(tableManager);
                        tableListener->release();
                    }
                    else
                    {
                        std::cout << "Cannot create request" << std::endl;
                        bWasError = true;
                    }
                }
                else
                {
                    std::cout << "Cannot get login rules" << std::endl;
                    bWasError = true;
                }
            }
            else
            {
                std::cout << "The Order '" << sampleParams->getOrderID() << "' is not valid" << std::endl;
                bWasError = true;
            }
        }
        else
        {
            std::cout << "No valid accounts" << std::endl;
            bWasError = true;
        }
        session->unsubscribeResponse(responseListener);
        responseListener->release();
        logout(session, sessionListener);
    }
    else
    {
        bWasError = true;
    }

    session->unsubscribeSessionStatus(sessionListener);
    sessionListener->release();
    session->release();

    delete loginParams;
    delete sampleParams;

    if (bWasError)
        return -1;
    return 0;
}

void printSampleParams(std::string &sProcName, LoginParams *loginParams, SampleParams *sampleParams)
{
    std::cout << "Running " << sProcName << " with arguments:" << std::endl;

    // Login (common) information
    if (loginParams)
    {
        std::cout << loginParams->getLogin() << " * "
                  << loginParams->getURL() << " "
                  << loginParams->getConnection() << " "
                  << loginParams->getSessionID() << " "
                  << loginParams->getPin() << std::endl;
    }

    // Sample specific information
    if (sampleParams)
    {
        std::cout << "OrderId='" << sampleParams->getOrderID() << "', "
                << "IsPegged='" << (sampleParams->IsPegged() ? "Yes" : "No") << "', "
                << "PegType='" << sampleParams->getPegType()<< "', "
                << "PegOffset='" << sampleParams->getPegOffset() << "', "
                << "Rate='" << sampleParams->getRate() << "', "
                << "Account='" << sampleParams->getAccount() << "'"
                << std::endl;
    }
}

void printHelp(std::string &sProcName)
{
    std::cout << sProcName << " sample parameters:" << std::endl << std::endl;

    std::cout << "/login | --login | /l | -l" << std::endl;
    std::cout << "Your user name." << std::endl << std::endl;

    std::cout << "/password | --password | /p | -p" << std::endl;
    std::cout << "Your password." << std::endl << std::endl;

    std::cout << "/url | --url | /u | -u" << std::endl;
    std::cout << "The server URL. For example, http://www.fxcorporate.com/Hosts.jsp." << std::endl << std::endl;

    std::cout << "/connection | --connection | /c | -c" << std::endl;
    std::cout << "The connection name. For example, \"Demo\" or \"Real\"." << std::endl << std::endl;

    std::cout << "/sessionid | --sessionid " << std::endl;
    std::cout << "The database name. Required only for users who have accounts in more than one database. Optional parameter." << std::endl << std::endl;

    std::cout << "/pin | --pin " << std::endl;
    std::cout << "Your pin code. Required only for users who have a pin. Optional parameter." << std::endl << std::endl;

    std::cout << "/orderid | --orderid" << std::endl;
    std::cout << "The identifier of the order to be stop attached." << std::endl << std::endl;

    std::cout << "/ispegged | --ispegged" << std::endl;
    std::cout << "Means that the price is specified as an offset (in pips) from pegoffset param" << std::endl << std::endl;

    std::cout << "/pegtype | --pegtype" << std::endl;
    std::cout << "The PegType. Possible values are: " << std::endl;
    std::cout << "O - The open price of the related trade " << std::endl;
    std::cout << "M - The close price(the current market price) of the related trade." << std::endl << std::endl;

    std::cout << "/pegoffset | --pegoffset" << std::endl;
    std::cout << "The offset to the price specified in the pegtype param. The offset must be expressed in pips." << std::endl << std::endl;

    std::cout << "/rate | --rate | /r | -r" << std::endl;
    std::cout << "The rate at which the order must be filled." << std::endl << std::endl;

    std::cout << "/account | --account " << std::endl;
    std::cout << "An account which you want to use in sample. Optional parameter." << std::endl << std::endl;
}

bool checkObligatoryParams(LoginParams *loginParams, SampleParams *sampleParams)
{
    /* Check login parameters. */
    if (strlen(loginParams->getLogin()) == 0)
    {
        std::cout << LoginParams::Strings::loginNotSpecified << std::endl;
        return false;
    }
    if (strlen(loginParams->getPassword()) == 0)
    {
        std::cout << LoginParams::Strings::passwordNotSpecified << std::endl;
        return false;
    }
    if (strlen(loginParams->getURL()) == 0)
    {
        std::cout << LoginParams::Strings::urlNotSpecified << std::endl;
        return false;
    }
    if (strlen(loginParams->getConnection()) == 0)
    {
        std::cout << LoginParams::Strings::connectionNotSpecified << std::endl;
        return false;
    }

    /* Check other parameters. */

    if (strlen(sampleParams->getOrderID()) == 0)
    {
        std::cout << SampleParams::Strings::orderidNotSpecified << std::endl;
        return false;
    }

    if (sampleParams->IsPegged() == true)
    {
        /* Check other parameters. */
        if (strlen(sampleParams->getPegType()) == 0)
        {
            std::cout << SampleParams::Strings::pegtypeNotSpecified<< std::endl;
            return false;
        }
        if (isNaN(sampleParams->getPegOffset()))
        {
            std::cout << SampleParams::Strings::pegoffsetNotSpecified<< std::endl;
            return false;
        }
    }
    else
    {
        if (isNaN(sampleParams->getRate()))
        {
            std::cout << SampleParams::Strings::rateNotSpecified << std::endl;
            return false;
        }
    }

    return true;
}

IO2GOrderTableRow *getOrder(IO2GTableManager *tableManager, const char *sOrderID)
{
    if (!tableManager || !sOrderID)
        return NULL;

    O2G2Ptr<IO2GOrdersTable> ordersTable = static_cast<IO2GOrdersTable *>(tableManager->getTable(Orders));

    IO2GOrderTableRow *order = NULL;
    IO2GTableIterator it;
    while (ordersTable->getNextRow(it, order))
    {
        if (strcmp(sOrderID, order->getOrderID()) == 0)
                return order;
        order->release();
    }
    return NULL;
}

IO2GRequest* attachStopOrderRequest(IO2GSession * session, IO2GOrderTableRow* order, const char* accountID, bool isPegged, const char* sPegType, double pegOffset, double rate)
{

    O2G2Ptr<IO2GRequestFactory> requestFactory = session->getRequestFactory();
    if (!requestFactory)
    {
        std::cout << "Cannot create request factory" << std::endl;
        return NULL;
    }
    bool buyOrder = false;
    if (!strcmp(order->getBuySell(), "B"))
        buyOrder = true;

    O2G2Ptr<IO2GValueMap> valuemap = requestFactory->createValueMap();
    valuemap->setString(OfferID, order->getOfferID());
    valuemap->setString(Command, O2G2::Commands::CreateOrder);
    valuemap->setString(OrderType, O2G2::Orders::Stop);
    valuemap->setString(AccountID, accountID);
    valuemap->setString(TradeID, order->getTradeID());
    valuemap->setString(BuySell, buyOrder ? "S" : "B");
    valuemap->setInt(Amount, order->getAmount());
    valuemap->setString(CustomID, "AttachStopOrder");
    if (isPegged)
    {
        valuemap->setDouble(PegOffset, buyOrder ? -pegOffset : pegOffset);
        valuemap->setString(PegType, sPegType);
    }
    else
    {
        valuemap->setDouble(Rate, rate);
    }

    O2G2Ptr<IO2GRequest> request = requestFactory->createOrderRequest(valuemap);
    if (!request)
    {
        std::cout << requestFactory->getLastError() << std::endl;
        return NULL;
    }
    return request.Detach();
}