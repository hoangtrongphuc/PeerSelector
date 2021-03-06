#include "libtorrent/pch.hpp"

#include <vector>
#include <limits>
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>


#include "libtorrent/BitSwiftSelector.hpp"

//longitude/latitude constants
const double NAUTICALMILEPERLATITUDE = 60.00721;
const double NAUTICALMILEPERLONGITUDE = 60.10793;

//rad = math.pi / 180.0
const double MILESPERNAUTICALMILE = 1.15078;
const double KMPERNAUTICALMILE = 1.852;

typedef ipAddress_s* ipAddress_s_ptr;
typedef rtt_s* rtt_s_ptr;
typedef asHop_s* asHop_s_ptr;

char* BitSwiftSelector::m_myIp  = NULL;

size_t BitSwiftSelector::write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    std::string buf = std::string(static_cast<char *>(ptr), size * nmemb);
    std::stringstream *response = static_cast<std::stringstream *>(stream);
    response->write(buf.c_str(), (std::streamsize)buf.size());
    BitSwiftSelector::m_myIp = NULL;
    BitSwiftSelector::m_myIp = new char[30];

	//std::vector<std::string> strs;
	//boost::split(strs, buf, boost::is_any_of(":"));
	//boost::split(strs, strs[1], boost::is_any_of("<"));
	// strcpy(BitSwiftSelector::m_myIp, strs[0].c_str());

    BitSwiftSelector::m_myIp = "213.101.214.227";
}

BitSwiftSelector::BitSwiftSelector() 
{
    CURL *curl;
    CURLcode res;
    std::stringstream response; 
    curl = curl_easy_init();
    if(curl) 
    {
        //curl_easy_setopt(curl, CURLOPT_URL, "http://www.whatismyip.org/");
//        curl_easy_setopt(curl, CURLOPT_URL, "http://api.hostip.info/get_html.php");
        curl_easy_setopt(curl, CURLOPT_URL, "http://checkip.dyndns.org/");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        res = curl_easy_perform(curl);
        
        /* always cleanup */ 
        curl_easy_cleanup(curl);
    }
    system("/bin/sh initialize.sh");
}

BitSwiftSelector::~BitSwiftSelector()
{ 
    if (BitSwiftSelector::m_myIp)
    {
         //delete[] BitSwiftSelector::m_myIp;
          //BitSwiftSelector::m_myIp = 0;
    }

    if (m_gi)
    {
        GeoIP_delete(m_gi); 
    }

    if (m_ASgi)
    {
        GeoIP_delete(m_ASgi); 
    }
} // Need to delete the pointer

double BitSwiftSelector::calculateDistance(double peerLatitude, double peerLongitude, double myLatitude, double myLongitude)
{
    double yDistance = abs(myLatitude - peerLatitude) * NAUTICALMILEPERLATITUDE;
    double xDistance = (cos(peerLatitude * (M_PI/180.0)) + cos(myLatitude * (M_PI/180.0))) * abs(myLongitude - peerLongitude) * (NAUTICALMILEPERLONGITUDE / 2);
    double distance = sqrt(pow(yDistance,2) + pow(xDistance,2));
    return int((distance * KMPERNAUTICALMILE) + .5);
}

void BitSwiftSelector::getRecords(char* ipAddress, string &continent, string & country, string &city)
{
     try
     {
	    GeoIPRecord  *gir = GeoIP_record_by_addr(m_gi, ipAddress);
	    string temp;
        if (gir)
        {
            if (gir->continent_code)
                continent = gir->continent_code;
            if (gir->country_name)
                country = gir->country_name;
            if (gir->city)
                city = gir->city;
            /*cout << "========================================================================="<<endl;
            cout << "Ip Address ->" <<ipAddress<<endl;
            cout << "Continent ->"<<continent<<endl;
            cout << "Country ->" <<country<<endl;
            cout << "City ->" <<city<<endl;
            cout << "========================================================================="<<endl;*/
            GeoIPRecord_delete(gir); 
        }

    }
    catch(...)
   {
   }
}

bool BitSwiftSelector::isInSameCity(char* peerIp)
{
    string continent;
    string country;
    string city;
    string myContinent;
    string myCountry;
    string myCity;
    getRecords(peerIp, continent, country, city);
    getRecords(BitSwiftSelector::m_myIp, myContinent, myCountry, myCity);
    if (city == myCity)
    {
        //cout <<"Peers are from same city"<<endl;
	    return true;
    }
    return false;
}

bool BitSwiftSelector::isInSameContinent(char* peerIp)
{
    string continent;
    string country;
    string city;
    string myContinent;
    string myCountry;
    string myCity;
    getRecords(peerIp, continent, country, city);
    getRecords(BitSwiftSelector::m_myIp, myContinent, myCountry, myCity);
    if (continent == myContinent)
    {
        //cout <<"Peers are from same continent"<<endl;
	    return true;
    }
    return false;
}

bool BitSwiftSelector::isInSameCountry(char* peerIp)
{
    string continent;
    string country;
    string city;
    string myContinent;
    string myCountry;
    string myCity;
    getRecords(peerIp, continent, country, city);
    getRecords(BitSwiftSelector::m_myIp, myContinent, myCountry, myCity);
    if (country == myCountry)
    {
        //cout <<"Peers are from same country"<<endl;
	    return true;
    }
    return false;

}
char* BitSwiftSelector::getProvider(char* ipAddress)
{
    try
    {
        // Wrong - need to buy ISP database. GeoIPISP.dat. is not free. sp m_ASgi should be replaced once we buy the database
	    char * org = GeoIP_org_by_name(m_ASgi, ipAddress);
        if (org)
        {
             return org;
        }
        else
        { 
            return NULL;
        }
    }
    catch(...)
    {
    }
}

bool BitSwiftSelector::isInSameProvider(char* peerIp)
{
    if (!strcmp(getProvider(peerIp),getProvider(BitSwiftSelector::m_myIp)))
    {
        cout <<"Peers are from same Provider"<<endl;
	    return true;
    }
    return false;
}
int BitSwiftSelector::getASN(const char* ipAddress)
{
		char* name = GeoIP_name_by_addr(m_ASgi, ipAddress);
		if (name == 0)
        {
            string errorStirng = "ASN is not known \n";
            cout << errorStirng;
            return 0;
        }

		// GeoIP returns the name as AS??? where ? is the AS-number
		return atoi(name + 2);
}

bool BitSwiftSelector::isInSameASN(char* peerIp)
{
 
    int a = getASN(peerIp);
    int b = getASN(BitSwiftSelector::m_myIp);
    if (a == b)
    {
        //cout <<"Peers are from same ASN"<<endl;
        return true;
    }
    return false;
}

int BitSwiftSelector::calculateScore(char* peerIp)
{
    double peerLatitude = 0;
    double peerLongitude = 0;
    double myLatitude = 0;
    double myLongitude = 0;
    int extraKm = 0;
    double totalScore = -1;
    //cout<<"================================================================================"<<endl;
    //cout<<"Peer IP address : "<< peerIp << endl;
    getCoordinates(peerIp, peerLatitude, peerLongitude);
    getCoordinates(BitSwiftSelector::m_myIp, myLatitude, myLongitude);
            
    double distance = calculateDistance(peerLatitude, peerLongitude, myLatitude, myLongitude);
    //cout << "Geographic distance between two peer:->" << distance << endl; 

    if (isInSameASN(peerIp))
    {
        extraKm = 1;
    }
    /*else if (isInSameProvider(peerIp))
    {
        extraKm = 10;    
    }*/
    else if (isInSameCity(peerIp))
    {
        extraKm = 100;
    }
    else if (isInSameCountry(peerIp))
    {
        extraKm = 200;
    }
    else if (isInSameContinent(peerIp))
    {
        extraKm = 300;
    }
    else
    {
        extraKm = 500;
    }       
    totalScore = distance + extraKm;
    return totalScore;
}

void BitSwiftSelector::initialize()
{
    m_gi = GeoIP_open("/usr/share/GeoIP/GeoIPCity.dat", GEOIP_MEMORY_CACHE);
    if (m_gi == NULL) {
        fprintf(stderr, "Error opening database\n");
	exit(1);
    }
    
    m_ASgi = GeoIP_open("/usr/share/GeoIP/GeoIPASNum.dat", GEOIP_STANDARD);
    if (m_ASgi == NULL) {
        fprintf(stderr, "Error opening database\n");
	exit(1);
    }
}

void BitSwiftSelector::getCoordinates(char *ip, double &latitude, double & longitude)
{
    try
    {
        GeoIPRecord    *gir = GeoIP_record_by_addr(m_gi, ip);
        if (gir)
        {
            //cout <<"Latitude : " << gir->latitude << endl;
            //cout <<"Longitude :" << gir->longitude << endl;
            latitude = gir->latitude;
            longitude = gir->longitude;
        }
    }
    catch (...)
    {
    }    
}

void BitSwiftSelector::sortIP(ipAddress_s ipAddressList[], int length)
{
     ipAddress_s temp;

     for(int i = 1; i < length - 1; i++)
     {
          for (int j = i + 1; j < length; j++)
          {
               if (ipAddressList[i].score > ipAddressList[j].score)  //comparing score
               {
                     temp = ipAddressList[i];    //swapping entire struct
                     ipAddressList[i] = ipAddressList[j];
                     ipAddressList[j] = temp;
               }
          }
     }
     return;
}


void BitSwiftSelector::sortRTT(rtt_s rttList[], int length)
{
     rtt_s temp;

     for(int i = 1; i < length - 1; i++)
     {
          for (int j = i + 1; j < length; j++)
          {
               if (rttList[i].rtt > rttList[j].rtt)  //comparing rtt
               {
                     temp = rttList[i];    //swapping entire struct
                     rttList[i] = rttList[j];
                     rttList[j] = temp;
               }
          }
     }
     return;
}

void BitSwiftSelector::sortAsHop(asHop_s asHopList[], int length)
{
     asHop_s temp;

     for(int i = 1; i < length - 1; i++)
     {
          for (int j = i + 1; j < length; j++)
          {
               if (asHopList[i].hopcount> asHopList[j].hopcount)  //comparing hop count
               {
                     temp = asHopList[i];    //swapping entire struct
                     asHopList[i] = asHopList[j];
                     asHopList[j] = temp;
               }
          }
     }
     return;
}
//calculated baed on Top Down Approach Book
double BitSwiftSelector::calculateRtt(char* ipAddress)
{

    FILE *fp;
    char rttBuffer[200];
    double SampleRTT = 0;
    double EstimatedRTT = 0;
    double Deviation = 0;
    double RTO = -1;
    double x = .1;

	mysqlpp::Connection conn(false);
	mysqlpp::Query query = conn.query(); // get an object from Querry class
	if (conn.connect("", HOST, USERNAME, PASSWORD))
	{
		if(!conn.select_db(DB))
		{
			conn.create_db(DB); // if no database, then create it first.
			conn.select_db(DB); // select database;
		}

		query << "SELECT rtt FROM rtt_table WHERE ipaddr = \"" << ipAddress << "\"";
		if (mysqlpp::StoreQueryResult res = query.store()) {
	   		mysqlpp::StoreQueryResult::const_iterator it;
   			int i = 0;
   			for (it = res.begin(); it != res.end(); ++it) {
				mysqlpp::Row row = *it;
			  	return row["rtt"];
   			}
  		}

	}
    for (int i = 0; i < 3; i++)
    {
        char cmd[50] = "/bin/sh rtt.sh ";
        strcat(cmd, ipAddress);
         
        fp = popen(cmd, "r");

        if (fp == NULL) 
        {
            cout << "Failed to run command"<< endl;
            exit(1);
        }

        /* Read the output a line at a time - output it. */
		fgets(rttBuffer, sizeof(rttBuffer)-1, fp);
        /*while (fgets(rttBuffer, sizeof(rttBuffer)-1, fp) != NULL) 
        	{
            		cout << "RTT ->  " << rttBuffer;
        	}*/
        SampleRTT = atof(rttBuffer);
		if (SampleRTT == 0)
		{
			break;
		}
        if (RTO == -1) 
        {
            EstimatedRTT = SampleRTT;
            Deviation = SampleRTT / 2.0;
            RTO = EstimatedRTT + 4 * Deviation;
        } 
        else 
        {
            // since the IP address is unreachable 2nd time, so there is no point trying again 
            if (EstimatedRTT == 0)
                break;
            Deviation = (1 - x) * Deviation + x * abs(EstimatedRTT - SampleRTT);
		    EstimatedRTT = (1 - x) * EstimatedRTT + x * SampleRTT;
            // This time out is for future use, right now I am relying on ping protocol for timeout
            RTO = EstimatedRTT + 4 * Deviation;
        }

        pclose(fp);
    }
    return EstimatedRTT;
}

//calculated baed on Top Down Approach Book
int BitSwiftSelector::calculateHopCount(char* ipAddress1, char* ipAddress2)
{

    FILE *fp;
    char asHopBuffer[10];
    if (ipAddress1 == 0 || ipAddress2 == 0)
    {
        cout << "ERROR : IP address is NULL"<< endl;
        return -1;
    }
	
	mysqlpp::Connection conn(false);
	mysqlpp::Query query = conn.query(); // get an object from Querry class
	if (conn.connect("", HOST, USERNAME, PASSWORD))
	{
		if(!conn.select_db(DB))
		{
			conn.create_db(DB); // if no database, then create it first.
			conn.select_db(DB); // select database;
		}

		query << "SELECT hopcount FROM ashop_table WHERE ipaddr = \"" << ipAddress2 << "\"";
		if (mysqlpp::StoreQueryResult res = query.store()) {
	   		mysqlpp::StoreQueryResult::const_iterator it;
   			int i = 0;
   			for (it = res.begin(); it != res.end(); ++it) {
				mysqlpp::Row row = *it;
			  	return row["hopcount"];
   			}
  		}

	}
    char cmd[50] = "/bin/sh as-hop.sh ";
    strcat(cmd, ipAddress1);
	strcat(cmd, " ");
    strcat(cmd, ipAddress2);
         
    fp = popen(cmd, "r");

    if (fp == NULL) 
    {
        cout << "ERROR : Failed to run command"<< endl;
        return -1;
    }

    /* Read the output a line at a time - output it. */
	fgets(asHopBuffer, sizeof(asHopBuffer)-1, fp);
    pclose(fp);   
    return atoi(asHopBuffer);
}
void BitSwiftSelector::getIpAddress(std::vector<ipPort_s> &list, string hash)
{
    ipPort_s addr;
    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            conn.create_db(DB); // if no database, then create it first.
            conn.select_db(DB); // select database;
        }

        query << "SELECT ipaddr,port FROM infohash_ipaddr WHERE infohash = \"" << hash << "\"";
        if (mysqlpp::StoreQueryResult res = query.store()) {
			cout << "We have:" << endl;
			mysqlpp::StoreQueryResult::const_iterator it;
			int i = 0;
			for (it = res.begin(); it != res.end(); ++it) {
				mysqlpp::Row row = *it;
				//cout << '\t' << row["ipaddr"] << "   " << row["port"] <<endl;
                addr.ipAddress = row["ipaddr"].c_str();
	            addr.port = row["port"];
		    	list.push_back(addr);
			}
		}
		else {
			cerr << "Failed to get item list: " << query.error() << endl;
		}
    }
}

void BitSwiftSelector::storePeerInScoreTable(ipAddress_s ipAddressList[], int length, string hash)
{
    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            conn.create_db(DB); // if no database, then create it first.
            conn.select_db(DB); // select database;
        }
        
        mysqlpp::Query ifTableExist = conn.query("describe score_table");
        
        if (!ifTableExist.execute())
        {
            query << "CREATE TABLE score_table (ipaddr VARCHAR(156) not null, port INT not null, score DOUBLE not null, infohash VARCHAR(40) not null, PRIMARY KEY (ipaddr, infohash), UNIQUE (ipaddr, infohash))";
            if(query.execute()) // execute it!
            {
                for (int i = 0; i < length; i++)
                {
                    query.reset();
                    query << "INSERT INTO score_table(ipaddr, port, score, infohash) VALUES (\"" << ipAddressList[i].ipAddress << "\", \"" << ipAddressList[i].port << "\", \"" << ipAddressList[i].score << "\" , \"" << hash << "\")";
                    query.execute();
                }
            }
            else
            {
               cerr << "Error : Table creation Failed: " << query.error() << endl;             
            }
        }
        else
        {
            mysqlpp::Connection conn(false);
            mysqlpp::Query query = conn.query(); // get an object from Querry class
            if (conn.connect("", HOST, USERNAME, PASSWORD)) 
            {
                if(!conn.select_db(DB))
                {
                    conn.create_db(DB); // if no database, then create it first.
                    conn.select_db(DB); // select database;
                }
                //Table is already present
                for (int i = 0; i < length; i++)
                {
                    query.reset();
                    query << "INSERT INTO score_table(ipaddr, port, score, infohash) VALUES (\""<<ipAddressList[i].ipAddress<<"\",\""<<ipAddressList[i].port<<"\",\""<<ipAddressList[i].score<<"\",\""<<hash<<"\")";
                    if (!query.execute())
                    {
                        query.reset();
                        query << "UPDATE score_table SET score = (\""<<ipAddressList[i].score<<"\") where ipaddr = (\""<<ipAddressList[i].ipAddress<<"\") AND port = (\""<<ipAddressList[i].port<<"\") AND infohash = (\""<<hash<<"\")";
                        query.execute();
                    }
                }
            }
        }
    }
    else
    {
        cout<<"Error : Not able to Establish connection to database"<<endl;
    }
}
void BitSwiftSelector::storePeerInAsHopTable(asHop_s asHopList[], int length, string hash)
{
    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            conn.create_db(DB); // if no database, then create it first.
            conn.select_db(DB); // select database;
        }
        mysqlpp::Query ifTableExist = conn.query("describe ashop_table");
        if (!ifTableExist.execute())
        {
            query << "CREATE TABLE ashop_table (ipaddr VARCHAR(156) not null, port INT not null, hopcount INT not null, infohash VARCHAR(40) not null, PRIMARY KEY (ipaddr, infohash), UNIQUE (ipaddr, infohash))";
            if(query.execute()) // execute it!
            {
                for (int i = 0; i < length; i++)
                {
                    query.reset();
                    query << "INSERT INTO ashop_table(ipaddr, port, hopcount, infohash) VALUES (\"" << asHopList[i].ipAddress << "\", \"" << asHopList[i].port << "\", \"" << asHopList[i].hopcount << "\" , \"" << hash << "\")";
                    query.execute();
                }
            }
            else
            {
               cerr << "Error : Table creation Failed: " << query.error() << endl;             
            }
        }
        else
        {
            mysqlpp::Connection conn(false);
            mysqlpp::Query query = conn.query(); // get an object from Querry class
            if (conn.connect("", HOST, USERNAME, PASSWORD)) 
            {
                if(!conn.select_db(DB))
                {
                    conn.create_db(DB); // if no database, then create it first.
                    conn.select_db(DB); // select database;
                }
                for (int i = 0; i < length; i++)
                {
                    query.reset();
                    query << "INSERT INTO ashop_table(ipaddr, port, hopcount, infohash) VALUES (\""<<asHopList[i].ipAddress<<"\",\""<<asHopList[i].port<<"\",\""<<asHopList[i].hopcount<<"\",\""<<hash<<"\")";
                    if (!query.execute())
                    {
                        query.reset();
                        query << "UPDATE ashop_table SET hopcount = (\""<<asHopList[i].hopcount<<"\") where ipaddr = (\""<<asHopList[i].ipAddress<<"\") AND port = (\""<<asHopList[i].port<<"\") AND infohash = (\""<<hash<<"\")";
                        query.execute();
                    }
                }
            }
        }
    }
    else
    {
        cout<<"Error : Not able to Establish connection to database"<<endl;
    }
}

void BitSwiftSelector::storePeerInRttTable(rtt_s ipAddressList[], int length, string hash)
{

	mysqlpp::Connection conn1(false);
    mysqlpp::Query query = conn1.query(); // get an object from Querry class
    if (conn1.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn1.select_db(DB))
        {
            conn1.create_db(DB); // if no database, then create it first.
            conn1.select_db(DB); // select database;
        }
        
        mysqlpp::Query ifTableExist = conn1.query("describe rtt_table");
        
        if (!ifTableExist.execute())
        {
            query << "CREATE TABLE rtt_table (ipaddr VARCHAR(156) not null, port INT not null, rtt DOUBLE not null, infohash VARCHAR(40) not null, PRIMARY KEY (ipaddr, infohash), UNIQUE (ipaddr, infohash))";
            if(query.execute()) // execute it!
            {
                for (int i = 0; i < length; i++)
                {
                    query.reset();
                    query << "INSERT INTO rtt_table(ipaddr, port, rtt, infohash) VALUES (\"" << ipAddressList[i].ipAddress << "\", \"" << ipAddressList[i].port  << "\", \"" << ipAddressList[i].rtt << "\" , \"" << hash << "\")";
                    query.execute();
					cout << " i = " << i << "length " <<length <<endl;
                }
            }
            else
            {
                cerr << "Error : Table creation Failed: " << query.error() << endl; 
            }

			
        }
        else
        {
            mysqlpp::Connection conn1(false);
            mysqlpp::Query query = conn1.query(); // get an object from Querry class
            if (conn1.connect("", HOST, USERNAME, PASSWORD)) 
            {
                if(!conn1.select_db(DB))
                {
                    conn1.create_db(DB); // if no database, then create it first.
                    conn1.select_db(DB); // select database;
                }
                //Table is already present
                for (int i = 0; i < length; i++)
                {
                    query.reset();
                    query << "INSERT INTO rtt_table(ipaddr, port, rtt, infohash) VALUES (\""<<ipAddressList[i].ipAddress<<"\",\""<<ipAddressList[i].port<<"\",\""<<ipAddressList[i].rtt<<"\",\""<<hash<<"\")";
                    if (!query.execute())
                    {

					    //cout << "crash1";
                        //query.reset();
                        //query << "SELECT rtt FROM rtt_table WHERE ipaddr = \"" << ipAddressList[i].ipAddress << "\" AND port = \"" << ipAddressList[i].port << "\" AND infohash = \"" << hash << "\"";
                        //if (mysqlpp::StoreQueryResult res = query.store()) 
                       // {
                            //if (abs(ipAddressList[i].rtt - res[0][0]) > 50)
                            {
                                cout <<"crash2";
                                query.reset();
                                query << "UPDATE rtt_table SET rtt = (\""<<ipAddressList[i].rtt<<"\") where ipaddr = (\""<<ipAddressList[i].ipAddress<<"\") AND port = (\""<<ipAddressList[i].port<<"\") AND infohash = (\""<<hash<<"\")";
								query.execute();
                            }
			                
		                //}
		                //else 
		                //{
			             //   cerr << "Failed to get item list: " << query.error() << endl;
		                //}
                    }


					cout << " i = " << i << "length " <<length <<endl;
                }
            }
        }
    }
    else
    {
        cout<<"Error : Not able to Establish connection to database"<<endl;
    }
}

void BitSwiftSelector::getPeerImmediately(string hash, int type, std::vector<ipPort_s> &ip_port, int number)
{
    int criteria; 
    string file_hash; 
    criteria = type;
    file_hash = hash;
    ipPort_s addr;
	int loopcount = 0;
	
    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            cerr << "Database is not present, So Urgent view is empty" << query.error() << endl;
        }

        if (criteria == 0)
        {
            query << "SELECT ipaddr,port FROM score_table WHERE infohash = \"" << file_hash << "\" ORDER BY score";
        }
		else if (criteria == 1)
		{
            query << "SELECT ipaddr,port FROM rtt_table WHERE infohash = \"" << file_hash << "\" ORDER BY rtt";
		}
		else if (criteria == 2)
		{
            query << "SELECT ipaddr,port FROM ashop_table WHERE infohash = \"" << file_hash << "\" ORDER BY hopcount";
		}
		else if (criteria == 3)
		{
	        query << "SELECT ipaddr,port FROM infohash_ipaddr WHERE infohash = \"" << file_hash << "\"";
		}
		else 
        {
			cerr << "Type can only be 0/1" << endl;
		}
        if (mysqlpp::StoreQueryResult res = query.store()) 
        {
			mysqlpp::StoreQueryResult::const_iterator it;
			if (number == 0)
			{
			    for (it = res.begin(); it != res.end(); ++it)
                {
				    mysqlpp::Row row = *it;
				    addr.ipAddress = const_cast<char *>(row["ipaddr"].c_str());
	                addr.port = row["port"];
        		    ip_port.push_back(addr);
			    }
            }
			else if (number > 0)
        	{
				for (it = res.begin(); it != res.end() && loopcount < number; ++it)
                {
				    mysqlpp::Row row = *it;
				   	addr.ipAddress = const_cast<char *>(row["ipaddr"].c_str());
					addr.port = row["port"];
                    ip_port.push_back(addr);
					loopcount++;
			    }
            }
			
		}
		else 
        {
            cerr << "Failed to get item list: " << query.error() << endl;
		}
    }
	return;
}

void BitSwiftSelector::getpeer(string hash, int type, std::vector<ipPort_s> &ip_port, int number) 
{
    // call geolitecityupdate.sh and geoIPASNum.sh to get MaxMind database
    system("/bin/sh geolitecityupdate.sh");
    system("/bin/sh geoIPASNum.sh");

    std::vector<ipPort_s> list;
    int listLength = 0;

   	std::cout << "before getIpAddress" << std::endl;
    getIpAddress(list, hash);
    initialize();
    listLength = list.size();
    // Calculate best peer based on Geographic location
    if (type == 0)
    {
        ipAddress_s_ptr ipAddressList;
        ipAddressList =  new ipAddress_s[listLength + 1];
        ipAddressList[0].ipAddress = BitSwiftSelector::m_myIp;
        ipAddressList[0].score = 0;
		ipAddressList[0].port = 0;
        int length = listLength + 1;
        for (int i = 0, j = 1; i < listLength; i ++)
        {
            char *peerIpAddress = const_cast<char *>(list[i].ipAddress.c_str());
            double peerScore = calculateScore(peerIpAddress);
            if (peerScore > 0)
            {
                ipAddressList[j].score = peerScore;
                ipAddressList[j].ipAddress = const_cast<char *>(list[i].ipAddress.c_str());
				ipAddressList[j].port = list[i].port;
                j++;
            }
            /*else
            {
                length = length - 1;
            }*/
			length = j - 1;
        }

        cout<<"================================================================================"<<endl;
        cout<<"Peers profiling based on score policy\n\n"<<endl;
        sortIP(ipAddressList, length);
        storePeerInScoreTable(ipAddressList, length, hash);
		if (number != 0 && length > number)
			length = number + 1;
        ipPort_s addr;
        for (int i = 1; i < length; i++)
        {
            cout<<"Peer :"<< ipAddressList[i].ipAddress <<":"<<ipAddressList[i].port<<" -> Score :"<< ipAddressList[i].score << endl;
        	addr.ipAddress = ipAddressList[i].ipAddress;
            addr.port = ipAddressList[i].port;
		    ip_port.push_back(addr);
        }
        cout<<"================================================================================"<<endl;
    }
    // calculate best peer based on RTT
    else if (type == 1)
    {
        rtt_s_ptr rttList, invalidList;
        rttList =  new rtt_s[listLength + 1];
        rttList[0].ipAddress = BitSwiftSelector::m_myIp;
        rttList[0].rtt = 0;
        rttList[0].port = 0;
        int rttlength = 1, invalidlength = 0;
        for (int i = 0, j = 0, k = 1; i < listLength; i ++)
        {
            char *peerIpAddress = const_cast<char *>(list[i].ipAddress.c_str());
            double rtt = calculateRtt(peerIpAddress);
            if (rtt > 0)
            {
                rttList[k].rtt = rtt;
                rttList[k].ipAddress = const_cast<char *>(list[i].ipAddress.c_str());
                rttList[k].port = list[i].port;
            	k++;
				rttlength = k;
            }
            /*else
        	{
        	    // Peer is not rechable, add invalid entry to identify not rechable peers
			    invalidList[j].rtt = -1;
                invalidList[j].ipAddress = const_cast<char *>(list[i].ipAddress.c_str());
                invalidList[j].port = list[i].port;
                invalidlength = j;
				j++;
        	}*/

        }

        
        cout<<"================================================================================"<<endl;
        cout<<"Best peer based on rtt"<<endl;
        sortRTT(rttList, rttlength);
        storePeerInRttTable(rttList, rttlength, hash);
		//storePeerInRttTable(invalidList, invalidlength, hash);
		if (number != 0 && rttlength > number)
			rttlength = number + 1;
        ipPort_s addr;
        for (int i = 1; i < rttlength; i ++)
        {
            cout<<"Peer :"<< rttList[i].ipAddress <<" -> RTT :"<< rttList[i].rtt << endl;
        	addr.ipAddress = rttList[i].ipAddress;
            addr.port = rttList[i].port;
		    ip_port.push_back(addr);
        }
    }
    if (type == 2)
    {
        asHop_s_ptr asHopList;
        asHopList =  new asHop_s[listLength + 1];
        asHopList[0].ipAddress = BitSwiftSelector::m_myIp;
        asHopList[0].hopcount= 0;
		asHopList[0].port = 0;
        int length = listLength + 1;
        for (int i = 0, j = 1; i < listLength; i ++)
        {
            char *peerIpAddress = const_cast<char *>(list[i].ipAddress.c_str());
            int hopcount = calculateHopCount(BitSwiftSelector::m_myIp, peerIpAddress);
            if (hopcount >= 0)
            {
                asHopList[j].hopcount = hopcount;
                asHopList[j].ipAddress = const_cast<char *>(list[i].ipAddress.c_str());
				asHopList[j].port = list[i].port;
                j++;
				length = j;
            }
            /*else
            {
                length = length - 1;
            }*/
        }
		
        cout<<"================================================================================"<<endl;
        cout<<"Best peer based on AS-Hop Count"<<endl;
        sortAsHop(asHopList, length);
        storePeerInAsHopTable(asHopList, length, hash);
		if (number != 0 && length > number)
			length = number + 1;
        ipPort_s addr;
        for (int i = 1; i < length; i++)
        {
            cout<<"Peer :"<< asHopList[i].ipAddress <<" -> Hop Count :"<< asHopList[i].hopcount<< endl;
        	addr.ipAddress = asHopList[i].ipAddress;
            addr.port = asHopList[i].port;
		    ip_port.push_back(addr);
        }
    }
	if (type == 3)
    {
		if (number != 0 && listLength > number)
			listLength = number;
        ipPort_s addr;
        for (int i = 0; i < listLength; i++)
        {
            cout<<"Peer :"<< list[i].ipAddress <<" -> Port :"<< list[i].port << endl;
        	addr.ipAddress = list[i].ipAddress;
            addr.port = list[i].port;
		    ip_port.push_back(addr);
        }
    }
}
void BitSwiftSelector::addpeers(std::vector<ipPort_s> ipPortList, string hash)
{
    std::cout<<"Before";
    initialize();
	std::cout<<"After";

    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            conn.create_db(DB); // if no database, then create it first.
            conn.select_db(DB); // select database;
        }
        
        mysqlpp::Query ifTableExist = conn.query("describe infohash_ipaddr");
        
        if (!ifTableExist.execute())
        {
            query << "CREATE TABLE infohash_ipaddr (ipaddr VARCHAR(36) not null , port INT not null , infohash VARCHAR(40) not null, asn INT not null, PRIMARY KEY (ipaddr, infohash))";
            if(query.execute()) // execute it!
            {
                for (std::vector<ipPort_s>::const_iterator j = ipPortList.begin(); j != ipPortList.end(); ++j)
        		{
       		        int asn = getASN(j->ipAddress.c_str());
                    query.reset();
                    query << "INSERT INTO infohash_ipaddr(ipaddr, port, infohash, asn) VALUES (\"" << j->ipAddress<< "\", \"" << j->port << "\" , \"" << hash << "\" , \"" << asn << "\")";
                    query.execute();
                }
            }
            else
            {
                cerr << "Error : Table creation Failed: " << query.error() << endl; 
            }
        }
        else
        {
            //Table is already present
            mysqlpp::Connection conn(false);
            mysqlpp::Query query = conn.query(); // get an object from Querry class
            if (conn.connect("", HOST, USERNAME, PASSWORD)) 
            {
                if(!conn.select_db(DB))
                {
                    conn.create_db(DB); // if no database, then create it first.
                    conn.select_db(DB); // select database;
                }
            }
			
			for (std::vector<ipPort_s>::const_iterator j = ipPortList.begin(); j != ipPortList.end(); ++j)
			{
		        int asn = getASN(j->ipAddress.c_str());
	            query.reset();
	            query << "INSERT INTO infohash_ipaddr(ipaddr, port, infohash, asn) VALUES (\""<<j->ipAddress<<"\",\""<<j->port<<"\",\""<<hash<<"\",\"" << asn << "\")";
	            query.execute();
			}
        }
    }
    else
    {
        cout<<"Error : Not able to Establish connection to database"<<endl;
    }

}

void BitSwiftSelector::addpeer(string ip, int port, string hash)
{
    /* can be used in future if we get list of peers to add, right now we are getting peer one by one
    ipPort_s ipPort;
    ipPort.ipAddress = ip;
    ipPort.port = port;
    ipPortList.push_back(ipPort);*/
    initialize();
    int asn = getASN(ip.c_str());

    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            conn.create_db(DB); // if no database, then create it first.
            conn.select_db(DB); // select database;
        }
        
        mysqlpp::Query ifTableExist = conn.query("describe infohash_ipaddr");
        
        if (!ifTableExist.execute())
        {
            query << "CREATE TABLE infohash_ipaddr (ipaddr VARCHAR(156) not null , port INT not null , infohash VARCHAR(40) not null, asn INT not null, PRIMARY KEY (ipaddr, infohash))";
            if(query.execute()) // execute it!
            {
                // We need not have to iterate the entire loop as of now because addpeer gets peer one by one
                //for (std::vector<ipPort_s>::const_iterator j = ipPortList.begin(); j != ipPortList.end(); ++j)
        		//{
                    query.reset();
                    query << "INSERT INTO infohash_ipaddr(ipaddr, port, infohash, asn) VALUES (\"" << ip << "\", \"" << port << "\" , \"" << hash << "\" , \"" << asn << "\")";
                    query.execute();
                //}
            }
            else
            {
                cerr << "Error : Table creation Failed: " << query.error() << endl; 
            }
        }
        else
        {
            //Table is already present
            mysqlpp::Connection conn(false);
            mysqlpp::Query query = conn.query(); // get an object from Querry class
            if (conn.connect("", HOST, USERNAME, PASSWORD)) 
            {
                if(!conn.select_db(DB))
                {
                    conn.create_db(DB); // if no database, then create it first.
                    conn.select_db(DB); // select database;
                }
            }
            query.reset();
            query << "INSERT INTO infohash_ipaddr(ipaddr, port, infohash, asn) VALUES (\""<<ip<<"\",\""<<port<<"\",\""<<hash<<"\",\"" << asn << "\")";
            query.execute();
        }
    }
    else
    {
        cout<<"Error : Not able to Establish connection to database"<<endl;
    }
}

bool BitSwiftSelector::compareBasedOnPS(string ip1, string ip2, string hash, int type)
{
    int criteria; 
    string file_hash; 
    criteria = type;
	int leftValue = 0, rightValue = 0;
	double leftRtt = 0, rightRtt= 0;
	int loopcount1 = 0, loopcount2 = 0;
	
    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            cerr << "Database is not present, So Urgent view is empty" << query.error() << endl;
        }

        if (criteria == 0)
        {
            query.reset();
            query << "SELECT score FROM score_table WHERE ipaddr = (\""<<ip1<<"\") AND infohash = (\""<<hash<<"\")";
			if (mysqlpp::StoreQueryResult res = query.store()) 
        	{
				mysqlpp::StoreQueryResult::const_iterator it;
			
		    	for (it = res.begin(); it != res.end(); ++it)
            	{
            	    loopcount1++;
			    	mysqlpp::Row row = *it;
			    	leftValue = row["score"];
				}
				if (loopcount1 == 0)
					leftValue = -1;
			}
            query.reset();
			query << "SELECT score FROM score_table WHERE ipaddr = (\""<<ip2<<"\") AND infohash = (\""<<hash<<"\")";
			if (mysqlpp::StoreQueryResult res = query.store()) 
        	{
				mysqlpp::StoreQueryResult::const_iterator it;
			
		    	for (it = res.begin(); it != res.end(); ++it)
            	{
            	    loopcount2++;
			    	mysqlpp::Row row = *it;
			    	rightValue = row["score"];
		    	}
				if (loopcount2 == 0)
					rightValue = -1;
			}
        }
		else if (criteria == 1)
		{
		
			query.reset();
			query << "SELECT rtt FROM rtt_table WHERE ipaddr = (\""<<ip1<<"\") AND infohash = (\""<<hash<<"\")";
			if (mysqlpp::StoreQueryResult res = query.store()) 
			{
				mysqlpp::StoreQueryResult::const_iterator it;
			
				for (it = res.begin(); it != res.end(); ++it)
				{
					loopcount1++;
					mysqlpp::Row row = *it;
					leftRtt = row["rtt"];
				}
				if (loopcount1 == 0)
					leftRtt = -1;
			}
			query.reset();
			query << "SELECT rtt FROM rtt_table WHERE ipaddr = (\""<<ip2<<"\") AND infohash = (\""<<hash<<"\")";

			if (mysqlpp::StoreQueryResult res = query.store()) 
			{
				mysqlpp::StoreQueryResult::const_iterator it;
			
				for (it = res.begin(); it != res.end(); ++it)
				{
					loopcount2++;
					mysqlpp::Row row = *it;
					rightRtt = row["rtt"];
				}
				if (loopcount2 == 0)
					rightRtt = -1;
			}
			
		    if (leftRtt == -1 && rightRtt >=0)
		    {
			    return false;
		    }
		    else if(leftRtt >= 0 && rightRtt == -1)
		    {
			    return true;
		    }
		    else if (leftRtt <= rightRtt)
		    {
			    return true;
		    }
		    else
		    {
			    return false;
		    }
		}
		else if (criteria == 2)
		{
			query.reset();
            query << "SELECT hopcount FROM ashop_table WHERE ipaddr = (\""<<ip1<<"\") AND infohash = (\""<<hash<<"\")";
			if (mysqlpp::StoreQueryResult res = query.store()) 
			{
				mysqlpp::StoreQueryResult::const_iterator it;
			
				for (it = res.begin(); it != res.end(); ++it)
				{
					loopcount1++;
					mysqlpp::Row row = *it;
					leftValue = row["hopcount"];
				}
				if (loopcount1 == 0)
					leftValue = -1;
			}
			query.reset();
            query << "SELECT hopcount FROM ashop_table WHERE ipaddr = (\""<<ip2<<"\") AND infohash = (\""<<hash<<"\")";

			if (mysqlpp::StoreQueryResult res = query.store()) 
			{
				mysqlpp::StoreQueryResult::const_iterator it;
			
				for (it = res.begin(); it != res.end(); ++it)
				{
					loopcount2++;
					mysqlpp::Row row = *it;
					rightValue = row["hopcount"];
				}
				if (loopcount2 == 0)
					rightValue = -1;
			}
		}
		else 
		{
			cerr << "Type can only be 0/1/2" << endl;
		}

		if (leftValue == -1 && rightValue >=0)
		{
			return false;
		}
		else if(leftValue >= 0 && rightValue == -1)
		{
			return true;
		}
		else if (leftValue <= rightValue)
		{
			return true;
		}
		else
		{
			return false;
		}
    }
}

void BitSwiftSelector::deletepeer(string ip, int port, string hash, int type)
{
    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            conn.create_db(DB); // if no database, then create it first.
            conn.select_db(DB); // select database;
        }
        mysqlpp::Query ifTableExist = conn.query("describe infohash_ipaddr");
        if (!ifTableExist.execute())
        {
        	cerr << "Error : Table does not exist: " << query.error() << endl; 
        }
        else
        {
            mysqlpp::Connection conn(false);
            mysqlpp::Query query = conn.query(); // get an object from Querry class
            if (conn.connect("", HOST, USERNAME, PASSWORD)) 
            {
                if(!conn.select_db(DB))
                {
                    conn.create_db(DB); // if no database, then create it first.
                    conn.select_db(DB); // select database;
                }
            }
            if (type == 100)
            {
				query.reset();
				query << "DELETE from infohash_ipaddr where ipaddr = (\""<<ip<<"\") AND port = (\""<<port<<"\") AND infohash = (\""<<hash<<"\")";
				if (query.execute())
				{
					query.reset();
					query << "DELETE from score_table where ipaddr = (\""<<ip<<"\") AND port = (\""<<port<<"\") AND infohash = (\""<<hash<<"\")";
					if(!query.execute())
			        	cerr << "Error : peer is not present in table: " << query.error() << endl; 
					query.reset();
					query << "DELETE from rtt_table where ipaddr = (\""<<ip<<"\") AND port = (\""<<port<<"\") AND infohash = (\""<<hash<<"\")";
					if (!query.execute())
			        	cerr << "Error : peer is not present in table: " << query.error() << endl; 
				}
				else
				{
		        	cerr << "Error : peer is not present in table: " << query.error() << endl; 
				}
            }
			else if(type == 0)
			{
				query.reset();
				query << "DELETE from score_table where ipaddr = (\""<<ip<<"\") AND port = (\""<<port<<"\") AND infohash = (\""<<hash<<"\")";
				if(!query.execute())
		        	cerr << "Error : peer is not present in table: " << query.error() << endl; 
			}
			else if(type == 1)
			{
				query.reset();
				query << "DELETE from rtt_table where ipaddr = (\""<<ip<<"\") AND port = (\""<<	port<<"\") AND infohash = (\""<<hash<<"\")";
				if (!query.execute())
			       	cerr << "Error : peer is not present in table: " << query.error() << endl; 
			}
			else
			{
				cerr << "Type can only be 0/1"<< endl;
			}
        }
    }
    else
    {
        cout<<"Error : Not able to Establish connection to database"<<endl;
    }
}
void BitSwiftSelector::deletepeers(string hash, int type)
{
    mysqlpp::Connection conn(false);
    mysqlpp::Query query = conn.query(); // get an object from Querry class
    if (conn.connect("", HOST, USERNAME, PASSWORD)) 
    {
        if(!conn.select_db(DB))
        {
            conn.create_db(DB); // if no database, then create it first.
            conn.select_db(DB); // select database;
        }
        mysqlpp::Query ifTableExist = conn.query("describe infohash_ipaddr");
        if (!ifTableExist.execute())
        {
        	cerr << "Error : Table does not exist: " << query.error() << endl; 
        }
        else
        {
            mysqlpp::Connection conn(false);
            mysqlpp::Query query = conn.query(); // get an object from Querry class
            if (conn.connect("", HOST, USERNAME, PASSWORD)) 
            {
                if(!conn.select_db(DB))
                {
                    conn.create_db(DB); // if no database, then create it first.
                    conn.select_db(DB); // select database;
                }
            }
            if (type == 100)
            {
				query.reset();
				query << "DELETE from infohash_ipaddr where infohash = (\""<<hash<<"\")";
				if (query.execute())
				{
					query.reset();
					query << "DELETE from score_table where infohash = (\""<<hash<<"\")";
					if(!query.execute())
			        	cerr << "Error : peer is not present in table: " << query.error() << endl; 
					query.reset();
					query << "DELETE from rtt_table where infohash = (\""<<hash<<"\")";
					if (!query.execute())
			        	cerr << "Error : peer is not present in table: " << query.error() << endl; 
				}
				else
				{
		        	cerr << "Error : peer is not present in table: " << query.error() << endl; 
				}
            }
			else if(type == 0)
			{
				query.reset();
				query << "DELETE from score_table where infohash = (\""<<hash<<"\")";
				if(!query.execute())
		        	cerr << "Error : peer is not present in table: " << query.error() << endl; 
			}
			else if(type == 1)
			{
				query.reset();
				query << "DELETE from rtt_table where infohash = (\""<<hash<<"\")";
				if (!query.execute())
			       	cerr << "Error : peer is not present in table: " << query.error() << endl; 
			}
			else
			{
				cerr << "Type can only be 0/1"<< endl;
			}
        }
    }
    else
    {
        cout<<"Error : Not able to Establish connection to database"<<endl;
    }
}
