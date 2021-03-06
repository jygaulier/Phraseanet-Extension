#include "base_header.h"


#include "../php_phrasea2/php_phrasea2.h"
#include "thread.h"
#include "sql.h"


CNODE *qtree2tree(zval **root, int depth); // in qtree.cpp
void freetree(CNODE *n); // in qtree.cpp
THREAD_ENTRYPOINT querytree2(void *_qp); // in qtree.cpp

bool pcanswercomp_rid_desc(PCANSWER lhs, PCANSWER rhs)
{
	return lhs->rid > rhs->rid;
}

bool pcanswercomp_int_asc(PCANSWER lhs, PCANSWER rhs)
{
	return (lhs->sortkey.l > rhs->sortkey.l);
}

bool pcanswercomp_int_desc(PCANSWER lhs, PCANSWER rhs)
{
	return (lhs->sortkey.l < rhs->sortkey.l);
}

bool pcanswercomp_str_asc(PCANSWER lhs, PCANSWER rhs)
{
	return ( (lhs->sortkey.s->compare(*(rhs->sortkey.s))) < 0);
}

bool pcanswercomp_str_desc(PCANSWER lhs, PCANSWER rhs)
{
	return ( (lhs->sortkey.s->compare(*(rhs->sortkey.s))) > 0);
}

bool pcanswercomp_sha(PCANSWER lhs, PCANSWER rhs)
{
	return ( *(lhs->sha2) > *(rhs->sha2));
}

ZEND_FUNCTION(phrasea_query2)
{
	long session, sbasid;
	zval *zqarray, *zcolllist;
	long userid;

	char *zsite;
	int zsitelen;

	long noCache = 0;
	long multidocMode = PHRASEA_MULTIDOC_DOCONLY;

	char *zsortfield = NULL;
	int zsortfieldlen;

//	zend_bool zbusiness = false;
	zval *zbusiness;
//	array_init(zbusiness);

	int sortorder = 0; // no sort
	int sortmethod = SORTMETHOD_STR;

//	char *sqlbusiness_c = NULL;

	char *zsrchlng = NULL;
	int zsrchlnglen;

	std::map<long, long> t_collid;			// key : distant_coll_id (dbox side) ==> value : local_base_id (appbox side)
	TCOLLMASK t_collmask; // key : distant_coll_id (dbox side) ==> value : mask_xor, mask_and

	switch(ZEND_NUM_ARGS())
	{
		case 8: // session, baseid, collist, quarray, site, userid, noCache, multidocMode
			if(zend_parse_parameters(8 TSRMLS_CC, (char *) "llaaslbl", &session, &sbasid, &zcolllist, &zqarray, &zsite, &zsitelen, &userid, &noCache, &multidocMode) == FAILURE)
			{
				RETURN_FALSE;
			}
			break;

		case 9:  // session, baseid, collist, quarray, site, userid, noCache, multidocMode, sortfield
			if(zend_parse_parameters(9 TSRMLS_CC, (char *) "llaaslbls", &session, &sbasid, &zcolllist, &zqarray, &zsite, &zsitelen, &userid, &noCache, &multidocMode, &zsortfield, &zsortfieldlen) == FAILURE)
			{
				RETURN_FALSE;
			}
			break;

		case 10: // session, baseid, collist, quarray, site, userid, noCache, multidocMode, sortfield, search_business
			if(zend_parse_parameters(10 TSRMLS_CC, (char *) "llaaslblsa", &session, &sbasid, &zcolllist, &zqarray, &zsite, &zsitelen, &userid, &noCache, &multidocMode, &zsortfield, &zsortfieldlen, &zbusiness) == FAILURE)
			{
				RETURN_FALSE;
			}
			break;

		case 11: // session, baseid, collist, quarray, site, userid, noCache, multidocMode, sortfield, search_business, srch_lng
			if(zend_parse_parameters(11 TSRMLS_CC, (char *) "llaaslblsas", &session, &sbasid, &zcolllist, &zqarray, &zsite, &zsitelen, &userid, &noCache, &multidocMode, &zsortfield, &zsortfieldlen, &zbusiness, &zsrchlng, &zsrchlnglen) == FAILURE)
			{
				RETURN_FALSE;
			}
			break;

		default:
			WRONG_PARAM_COUNT;	// returns !
			break;
	}


	if(ZEND_NUM_ARGS() >= 9)	// field 9 is "sortfield"
	{
		if(zsortfieldlen > 0)
		{
			sortorder = 1; // default : desc
			if(zsortfield[0] == '+')
			{
				zsortfield++;
				zsortfieldlen--;
				sortorder = -1;
			}
			else if(zsortfield[0] == '-')
			{
				zsortfield++;
				zsortfieldlen--;
			}
		}
		if(zsortfieldlen > 0)
		{
			sortmethod = SORTMETHOD_STR; // default : desc
			if(zsortfield[0] == '0')
			{
				zsortfield++;
				zsortfieldlen--;
				sortmethod = SORTMETHOD_INT;
			}
		}
		if(zsortfieldlen == 0)
			zsortfield = NULL;
	}
/*
	if(ZEND_NUM_ARGS() >= 10)	// field 10 is "search_business"
	{
		std::stringstream sqlbusiness_strm;
		sqlbusiness_strm << "OR FIND_IN_SET(record.coll_id, '";
		zval **tmp1;
		bool first = true;
		int n = 0;

		for(int i=0; true; i++)
		{
			if(zend_hash_index_find(HASH_OF(zbusiness), i, (void **) &tmp1) == SUCCESS)
			{
				if(Z_TYPE_PP(tmp1) == IS_LONG)
				{
					if(!first)
						sqlbusiness_strm << ",";
					sqlbusiness_strm << Z_LVAL_P(*tmp1);
					first = false;
					n++;
				}
			}
			else
				break;
		}
		if(n > 0)
		{
			sqlbusiness_strm << "')";
			std::string sqlbusiness_str = sqlbusiness_strm.str();
			if( (sqlbusiness_c = (char *)EMALLOC(sqlbusiness_str.length()+1)) != NULL)
				memcpy(sqlbusiness_c, sqlbusiness_str.c_str(), sqlbusiness_str.length()+1);
		}
		else
		{
			if( (sqlbusiness_c = (char *)EMALLOC(1)) != NULL)
				sqlbusiness_c[0] = '\0';
		}
	}
 */
	if(ZEND_NUM_ARGS() >= 11)	// field 1 is "srch_lng"
	{
	}


	SQLCONN *epublisher = PHRASEA2_G(epublisher);
	if(!epublisher)
	{
		// we need conn !
		//		zend_throw_exception(spl_ce_LogicException, "No connection set (check that phrasea_conn(...) returned true).", 0 TSRMLS_CC);
		RETURN_FALSE;
	}

	if(!PHRASEA2_G(global_session) || PHRASEA2_G(global_session)->get_session_id() != session)
	{
		// no session, can't continue
		RETURN_FALSE;
	}

	array_init(return_value);

	CHRONO time_phpfct;
	startChrono(time_phpfct);

	SQLRES res(epublisher);

	// replace list of 'local' collections  (base_ids) by list of 'distant' collections (coll ids)
	if(Z_TYPE_P(zcolllist) == IS_ARRAY)
	{
		// fill the distant_coll_id to local_base_id map
		for(int i = 0; true; i++)
		{
			zval **tmp1;
			if(zend_hash_index_find(HASH_OF(zcolllist), i, (void **) &tmp1) == SUCCESS)
			{
				if(Z_TYPE_PP(tmp1) == IS_LONG)
				{
					long distant_coll_id = PHRASEA2_G(global_session)->get_distant_coll_id(Z_LVAL_P(*tmp1));
					if(distant_coll_id != -1)
					{
						t_collid[distant_coll_id] = Z_LVAL_P(*tmp1);
					}
				}
			}
			else
				break;
		}

		if(t_collid.size() > 0)
		{
			CHRONO time_connect;
			startChrono(time_connect);

			// get the adresse of the distant database
			std::stringstream sql;
			// SECURITY : sbasid is type long, no risk of injection
			sql << "SELECT sbas_id, host, port, sqlengine, dbname, user, pwd FROM sbas WHERE sbas_id=" << sbasid;
add_assoc_string(return_value, (char *) "sql_sbas", ((char *) (sql.str().c_str())), true);
			if(res.query((char *) (sql.str().c_str())))
			{
				SQLROW *row = res.fetch_row();
				if(row)
				{
					// on se connecte sur la base distante
					SQLCONN *conn = new SQLCONN(row->field(1, "127.0.0.1"), atoi(row->field(2, "3306")), row->field(5, "root"), row->field(6, ""), row->field(4, "dbox"));
					if(conn && conn->connect())
					{
						add_assoc_double(return_value, (char *) "time_connect", stopChrono(time_connect));

						// build the sql that filter collections
						CHRONO time_tmpmask;
						startChrono(time_tmpmask);

						if(zsitelen > 256)
							zsitelen = 256;

						char zsite_esc[513];
						zsite_esc[conn->escape_string(zsite, zsitelen, zsite_esc)] = '\0';

						std::stringstream sqlcoll;
						// SECURITY : userid is type long
						sqlcoll << "CREATE TEMPORARY TABLE `_tmpmask` (PRIMARY KEY(coll_id), KEY(only_business)) ENGINE=MEMORY SELECT coll_id, mask_xor, mask_and, ";
                        
                        if(ZEND_NUM_ARGS() >= 10)	// field 10 is "search_business"
                        {
                            sqlcoll << " FIND_IN_SET(coll_id, '";
                            zval **tmp1;
                            bool first = true;
                            int n = 0;
                            
                            for(int i=0; true; i++)
                            {
                                if(zend_hash_index_find(HASH_OF(zbusiness), i, (void **) &tmp1) == SUCCESS)
                                {
                                    if(Z_TYPE_PP(tmp1) == IS_LONG)
                                    {
                                        if(!first)
                                        {
                                            sqlcoll << ",";
                                        }
                                        sqlcoll << Z_LVAL_P(*tmp1);
                                        first = false;
                                        n++;
                                    }
                                }
                                else
                                break;
                            }

                            sqlcoll << "')=0";
                        }
                        else
                        {
                            sqlcoll << " TRUE";
                        }

                        
                        sqlcoll << " AS only_business FROM collusr WHERE site='" << zsite_esc << "' AND usr_id=" << userid << " AND coll_id";
						
                        
                        
                        
                        if(t_collid.size() == 1)
						{
							// SECURITY : t_collid[i] is type long
							sqlcoll << '=' << t_collid.begin()->first;
						}
						else
						{
							// SECURITY : t_collid[i] is type long
							sqlcoll << " IN (-1"; // begin with a fake number (-1) allows to insert a comma each time (avoid a test)
							std::map<long, long>::iterator it;
							for(it = t_collid.begin(); it != t_collid.end(); it++)
								sqlcoll << ',' << it->first;
							sqlcoll << ')';
						}
						conn->query((char *) (sqlcoll.str().c_str())); // CREATE _tmpmask ...

                        add_assoc_string(return_value, (char *) "sql_tmpmask", (char *) (sqlcoll.str().c_str()), true);

						add_assoc_double(return_value, (char *) "time_tmpmask", stopChrono(time_tmpmask));
/*
						// small sql that joins record and collusr
						const char *sqltrec = "(record)";

						// load the tmpmask in ram
						SQLRES restmp1(conn);
						SQLROW *rowtmp1;
						restmp1.query("SELECT coll_id, `mask_xor`, `mask_and` FROM `_tmpmask`");
						while((rowtmp1 = restmp1.fetch_row()))
						{
							long coll_id  = atol(rowtmp1->field(0, "0"));
							unsigned long mask_xor = atol(rowtmp1->field(1, "0"));
							unsigned long mask_and = atol(rowtmp1->field(2, "0"));
							t_collmask[coll_id] = COLLMASK(mask_xor, mask_and);;
						}
*/

						// let's check if we need to include mask in our queries (if every mask on every coll is '0', we dont need)
                        bool use_mask = true;
						SQLRES restmp(conn);
						SQLROW *rowtmp;
						restmp.query("SELECT BIT_COUNT(BIT_OR(`mask_xor` | `mask_and`)) FROM `_tmpmask`");
						if((rowtmp = restmp.fetch_row()))
						{
							const char *f = rowtmp->field(0, "0");
							if(f[0] == '0' && f[1] == '\0')
							{
								// all masks=0; we don't need masks in queries !
                                use_mask = false;
/*
								if(multidocMode == PHRASEA_MULTIDOC_DOCONLY)
									sqltrec = "(record INNER JOIN _tmpmask ON _tmpmask.coll_id=record.coll_id AND parent_record_id=0)";
								else
									sqltrec = "(record INNER JOIN _tmpmask ON _tmpmask.coll_id=record.coll_id AND parent_record_id=1)";
							}
							else
							{
								// at least one mask is set, too bad we need _tmpmask in queries
								if(multidocMode == PHRASEA_MULTIDOC_DOCONLY)
									sqltrec = "(record INNER JOIN _tmpmask ON _tmpmask.coll_id=record.coll_id AND parent_record_id=0 AND ((status ^ mask_xor) & mask_and)=0)";
								else
									sqltrec = "(record INNER JOIN _tmpmask ON _tmpmask.coll_id=record.coll_id AND parent_record_id=1 AND ((status ^ mask_xor) & mask_and)=0)";
*/
                            }
						}

						// change the php query to a tree of nodes
						CNODE *query;
						query = qtree2tree(&zqarray, 0);

						char zsortfield_esc[65];
						char *psortfield_esc = zsortfield_esc;
						char **pzsortfield; // pass as ptr because querytree2 may reset it to null during exec of 'sha256=sha256'
						// SECURITY : escape zsortfield
						if(zsortfield != NULL)
						{
							if(zsortfieldlen > 32)
								zsortfieldlen = 32;
							zsortfield_esc[conn->escape_string(zsortfield, zsortfieldlen, zsortfield_esc)] = '\0';

							pzsortfield = &psortfield_esc; // pass as ptr because querytree2 may reset it to null during exec of 'sha256=sha256'
						}
						else
						{
							pzsortfield = &zsortfield; // pass as ptr because querytree2 may reset it to null during exec of 'sha256=sha256'
						}

						struct sb_stemmer *stemmer = NULL;
						// SECURITY : escape search_lng
						char srch_lng[33];
						char srch_lng_esc[65];

						srch_lng[0] = srch_lng_esc[0] = '\0';
						if(zsrchlng != NULL)
						{
							if(zsrchlnglen > 32)
								zsrchlnglen = 32;
							memcpy((void*)srch_lng, (void*)zsrchlng, zsrchlnglen);
							srch_lng[zsrchlnglen] = '\0';
							srch_lng_esc[conn->escape_string(srch_lng, zsrchlnglen, srch_lng_esc)] = '\0';

							stemmer = sb_stemmer_new(srch_lng, "UTF_8");
						}

						// here we query phrasea !
// pthread_mutex_t sqlmutex;
						CMutex sqlmutex;

						Cquerytree2Parm qp(
										 query,
										 'M',				// main thread
										 0,					// depth 0
										 conn,

										 (char *)row->field(1, "127.0.0.1"),	// host
										 atoi(row->field(2, "3306")),	// port
										 (char *)row->field(5, "root"),			// user
										 (char *)row->field(6, ""),				// pwd
										 (char *)row->field(4, "dbox"),			// base
								//		 (char *)"_tmpmask",						// tmptable

								//		 NULL,		// sqltmp

										 &sqlmutex,
										 return_value,
										 // sqltrec,
                                         multidocMode,          // parent_record_id
                                         use_mask,              // bool
										 /*t_collmask,*/
										 pzsortfield,
										 sortmethod,
										 // sqlbusiness_c,
										 stemmer,
										 srch_lng_esc,
								//		 0,				// 0: do not filter on rid
                                         NULL               // NULL : do not filter on rid
						);

						if(MYSQL_THREAD_SAFE)
						{
#ifdef WIN32
							HANDLE  hThread;
							unsigned dwThreadId;
							hThread = (HANDLE)_beginthreadex(
								NULL,                   // default security attributes
								0,                      // use default stack size
								querytree2,			    // thread function name
								(void*)&qp,             // argument to thread function
								0,                      // use default creation flags
								&dwThreadId);   // returns the thread identifier
							WaitForSingleObject(hThread, INFINITE);
							CloseHandle(hThread);
#else
							ATHREAD thread;
							THREAD_START(thread, querytree2, &qp);
							THREAD_JOIN(thread);
#endif
						}
						else
						{
							querytree2((void *) &qp);
						}

						if(stemmer)
							sb_stemmer_delete(stemmer);

						conn->query("DROP TABLE _tmpmask");	// no need to drop temporary tables, but clean

						if(!noCache)
						{
							// serialize in binary to write on cache files
							CANSWER *answer;
							CSPOT *spot;
							int n_answers = 0, n_spots = 0;
							bool sortBySha256 = false;

							// count answers, spots, ...
							// ...and check if any 'sha256' is returned (in case results will be sorted by sha256)
							CHRONO time_sort;
							startChrono(time_sort);

							std::multiset<PCANSWER, PCANSWERCOMPRID_DESC>::iterator it;
							for(it = query->answers.begin(); it != query->answers.end(); it++)
							{
								answer = *(it);
								n_answers++;
								answer->nspots = 0;
								for(spot = answer->firstspot; spot; spot = spot->_nextspot)
								{
									n_spots++;
									answer->nspots++;
								}
								if(answer->sha2)
									sortBySha256 = true;
							}

							std::multiset<PCANSWER, bool(*)(PCANSWER, PCANSWER)> *pmset = NULL;

							// if we need sort
							if(sortBySha256)
							{
								pmset = new std::multiset<PCANSWER, bool(*)(PCANSWER, PCANSWER)>(pcanswercomp_sha);
							}
							else if(zsortfield)
							{
								if(sortmethod == SORTMETHOD_STR)
								{
									if(sortorder >= 0)
										pmset = new std::multiset<PCANSWER, bool(*)(PCANSWER, PCANSWER)>(pcanswercomp_str_asc);
									else
										pmset = new std::multiset<PCANSWER, bool(*)(PCANSWER, PCANSWER)>(pcanswercomp_str_desc);
								}
								else
								{
									if(sortorder >= 0)
										pmset = new std::multiset<PCANSWER, bool(*)(PCANSWER, PCANSWER)>(pcanswercomp_int_asc);
									else
										pmset = new std::multiset<PCANSWER, bool(*)(PCANSWER, PCANSWER)>(pcanswercomp_int_desc);
								}
							}
							if(pmset)
								pmset->insert(query->answers.begin(), query->answers.end());

							add_assoc_double(return_value, (char *) "time_sort", stopChrono(time_sort));


							CHRONO time_writeCache;
							startChrono(time_writeCache);

							if(true)	// n_answers > 0)
							{
								int answer_binsize, spot_binsize;
								CACHE_ANSWER *answer_binbuff = NULL;
								CACHE_SPOT *spot_binbuff = NULL;

								answer_binsize = n_answers * sizeof (CACHE_ANSWER);
								spot_binsize = n_spots * sizeof (CACHE_SPOT);

								// to set the offset of spots, we must know how much are already in file
								unsigned int spot_index = 0;

								FILE *fp_spots = NULL, *fp_answers = NULL;
								char *fname;
								int l = strlen(PHRASEA2_G(tempPath))
									+ 9 // "_phrasea."
									+ strlen(epublisher->ukey)
									+ 9 // ".answers."
									+ 33 // session
									+ 1; // '\0'
								if((fname = (char *) EMALLOC(l)))
								{
									if(n_spots > 0)
									{
										sprintf(fname, "%s_phrasea.%s.spots.%ld.bin", PHRASEA2_G(tempPath), epublisher->ukey, session);
										if((fp_spots = fopen(fname, "ab")))
										{
											fseek(fp_spots, 0, SEEK_END);
											spot_index = ftell(fp_spots) / sizeof (CACHE_SPOT);
										}
									}
									sprintf(fname, "%s_phrasea.%s.answers.%ld.bin", PHRASEA2_G(tempPath), epublisher->ukey, session);
									if((fp_answers = fopen(fname, "ab")))
									{
										;
									}
									EFREE(fname);
								}

								if((answer_binsize == 0 || (answer_binbuff = (CACHE_ANSWER *) EMALLOC(answer_binsize))) && (spot_binsize == 0 || (spot_binbuff = (CACHE_SPOT *) EMALLOC(spot_binsize))))
								{
									CACHE_ANSWER *panswer = answer_binbuff;
									CACHE_SPOT *pspot = spot_binbuff;
									long current_cid = -1;
									long current_bid = -1;

									if(pmset)
									{
										// dump the sorted multiset to cache
										std::multiset<PCANSWER, bool(*)(PCANSWER, PCANSWER)>::iterator ipmset;
										for(ipmset = pmset->begin(); ipmset != pmset->end(); ipmset++)
										{
											answer = *(ipmset);
											if(answer->cid != current_cid)
											{
												// change of collection, search id of matching local base
												if((current_bid = t_collid[answer->cid]) == 0) // 0 if cid is unknown (default int constructor)
													current_bid = -1;
												current_cid = answer->cid;
											}

											panswer->rid = answer->rid;
											panswer->bid = current_bid;
											panswer->spots_index = spot_index;
											panswer->nspots = answer->nspots;
											spot_index += answer->nspots;
											for(spot = answer->firstspot; spot; spot = spot->_nextspot)
											{
												pspot->start = spot->start;
												pspot->len = spot->len;
												pspot++;
											}
											panswer++;
										}
										delete pmset;
									}
									else
									{
										// no sort, dump directly from query
										std::multiset<PCANSWER, PCANSWERCOMPRID_DESC>::iterator it;
										for(it = query->answers.begin(); it != query->answers.end(); it++)
										{
											answer = *(it);
											if(answer->cid != current_cid)
											{
												// change of collection, search id of matching local base
												if((current_bid = t_collid[answer->cid]) == 0) // 0 if cid is unknown (default int constructor)
													current_bid = -1;
												current_cid = answer->cid;
											}

											panswer->rid = answer->rid;
											panswer->bid = current_bid;
											panswer->spots_index = spot_index;
											panswer->nspots = answer->nspots;
											spot_index += answer->nspots;
											for(spot = answer->firstspot; spot; spot = spot->_nextspot)
											{
												pspot->start = spot->start;
												pspot->len = spot->len;
												pspot++;
											}
											panswer++;
										}
									}

									if(fp_answers)
									{
										if(answer_binbuff)
											fwrite((const void *) answer_binbuff, 1, answer_binsize, fp_answers);
										fclose(fp_answers);
									}
									if(fp_spots)
									{
										if(spot_binbuff)
											fwrite((const void *) spot_binbuff, 1, spot_binsize, fp_spots);
										fclose(fp_spots);
									}
//									EFREE(answer_binbuff);
								}
//								else
//								{
									// pb d'allocation !
									if(answer_binbuff)
										EFREE(answer_binbuff);
									if(spot_binbuff)
										EFREE(spot_binbuff);
//								}
							} // if(n_answers > 0)
							add_assoc_double(return_value, (char *) "time_writeCache", stopChrono(time_writeCache));

						} // if(!noCache)

						// free the tree
						freetree(query);
					}
					if(conn)
						delete conn;
				}
			}
		}
	}
/*
	if(sqlbusiness_c)
		EFREE(sqlbusiness_c);
*/
	add_assoc_double(return_value, (char *) "time_phpfct", stopChrono(time_phpfct));
    
    log("phrasea_query2", return_value);
}

