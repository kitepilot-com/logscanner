-----------------------
-- Ctrl_ReadPtr --
-----------------------
-- This table keeps the last position read in every logfile
-- so we can return to it when the program restarts.
-- there is one row for every logfile being read.
-- It also keeps the first line read on the file and restarts the system if that line chnges.
-- That way we don't need to touch logrotate...
-- DROP TABLE IF EXISTS Ctrl_ReadPtr;
-- CREATE TABLE Ctrl_ReadPtr(
-- 	Logfile    VARCHAR(1024) NOT NULL,
-- 	NextRead   INT           NOT NULL,
-- 	Logline    VARCHAR(3072) NOT NULL
-- );

-- -- Primary key
-- CREATE UNIQUE INDEX I010_Ctrl_ReadPtr ON Ctrl_ReadPtr(
-- 	Logline
-- );

---------------
-- T010_Eval --
---------------
-- Log lines matched by regexps with Severity_val >= 1 are inserted here for further eval.
-- Log lines are then deleted moved to Hist if dropped.
DROP TABLE IF EXISTS T010_Eval;
CREATE TABLE T010_Eval(
	LogTmstmp    VARCHAR(11)   NOT NULL,
	Octect_1     INT           NOT NULL, constraint Octect_1_HST check(Octect_1 >  0), constraint Octect_1_LST check(Octect_1 < 256),
	Octect_2     INT           NOT NULL, constraint Octect_2_HST check(Octect_2 >= 0), constraint Octect_2_LST check(Octect_2 < 256),
	Octect_3     INT           NOT NULL, constraint Octect_3_HST check(Octect_3 >= 0), constraint Octect_3_LST check(Octect_3 < 256),
	Octect_4     INT           NOT NULL, constraint Octect_4_HST check(Octect_4 >= 0), constraint Octect_4_LST check(Octect_4 < 256),
	Logfile      VARCHAR(1024) NOT NULL,
	Logline      VARCHAR(3072) NOT NULL,
	Rexexp       VARCHAR(1024) NOT NULL,
	Severity_val INT           NOT NULL,
	Timestamp    VARCHAR(20)   NOT NULL
);

-- Primary key
CREATE INDEX I010_T010_Eval ON T010_Eval(
	LogTmstmp,
	Octect_1,
	Octect_2,
	Octect_3,
	Octect_4,
	Timestamp
);

----------------
-- T020_Audit --
----------------
-- Log lines matched by several regexps are stored here for optimization.
DROP TABLE IF EXISTS T020_Audit;
CREATE TABLE T020_Audit(
	LogTmstmp    VARCHAR(11)   NOT NULL,
	Octect_1     INT           NOT NULL, constraint Octect_1_HST check(Octect_1 >  0), constraint Octect_1_LST check(Octect_1 < 256),
	Octect_2     INT           NOT NULL, constraint Octect_2_HST check(Octect_2 >= 0), constraint Octect_2_LST check(Octect_2 < 256),
	Octect_3     INT           NOT NULL, constraint Octect_3_HST check(Octect_3 >= 0), constraint Octect_3_LST check(Octect_3 < 256),
	Octect_4     INT           NOT NULL, constraint Octect_4_HST check(Octect_4 >= 0), constraint Octect_4_LST check(Octect_4 < 256),
	Logfile      VARCHAR(1024) NOT NULL,
	Logline      VARCHAR(3072) NOT NULL,
	Rexexp       VARCHAR(1024) NOT NULL,
	Severity_val INT           NOT NULL,
	multimatch   BOOL          NOT NULL,
	whitelisted  BOOL          NOT NULL
);

CREATE INDEX I010_T020_Audit ON T020_Audit(
	Octect_1,
	Octect_2,
	Octect_3,
	Octect_4,
	LogTmstmp
);

---------------
-- T030_Drop --
---------------
DROP TABLE IF EXISTS T030_Drop;
CREATE TABLE T030_Drop(
	Timestamp    VARCHAR(20)   NOT NULL,
	Octect_1     INT           NOT NULL, constraint Octect_1_HST check(Octect_1 >  0), constraint Octect_1_LST check(Octect_1 < 256),
	Octect_2     INT           NOT NULL, constraint Octect_2_HST check(Octect_2 >= 0), constraint Octect_2_LST check(Octect_2 < 256),
	Octect_3     INT           NOT NULL, constraint Octect_3_HST check(Octect_3 >= 0), constraint Octect_3_LST check(Octect_3 < 256),
	Octect_4     INT           NOT NULL, constraint Octect_4_HST check(Octect_4 >= 0), constraint Octect_4_LST check(Octect_4 < 256),
	ProcName     VARCHAR(1024) NOT NULL,
	LineOrTmstmp VARCHAR(3072),
	Status       CHAR          NOT NULL, constraint Status_VAL   check(Status = 'N' OR Status = 'P' OR Status = 'D')
--  Status(Status = 'New' OR Status = 'Pending' OR Status = 'Dropped')
);

-- Primary key
CREATE UNIQUE INDEX I010_T030_Drop ON T030_Drop(
	Octect_1,
	Octect_2,
	Octect_3,
	Octect_4
);

---------------
-- T040_Hist --
---------------
-- Dropped IPs are stored in this table for forensics.
-- Lines with Severity_val > 0 are inserted by the eval thread.
DROP TABLE IF EXISTS T040_Hist;
CREATE TABLE T040_Hist(
	LogTmstmp    VARCHAR(11)   NOT NULL,
	Octect_1     INT           NOT NULL, constraint Octect_1_HST check(Octect_1 >  0), constraint Octect_1_LST check(Octect_1 < 256),
	Octect_2     INT           NOT NULL, constraint Octect_2_HST check(Octect_2 >= 0), constraint Octect_2_LST check(Octect_2 < 256),
	Octect_3     INT           NOT NULL, constraint Octect_3_HST check(Octect_3 >= 0), constraint Octect_3_LST check(Octect_3 < 256),
	Octect_4     INT           NOT NULL, constraint Octect_4_HST check(Octect_4 >= 0), constraint Octect_4_LST check(Octect_4 < 256),
	Logfile      VARCHAR(1024) NOT NULL,
	Logline      VARCHAR(3072) NOT NULL,
	Rexexp       VARCHAR(1024) NOT NULL,
	Severity_val INT           NOT NULL,
	Timestamp    VARCHAR(20)   NOT NULL
);

--------------------
-- T100_LinesSeen --
--------------------
-- All the lines we have seen.
-- We care specifically about regexMatch == false.
DROP TABLE IF EXISTS T100_LinesSeen;
CREATE TABLE T100_LinesSeen(
	Timestamp  VARCHAR(20)   NOT NULL,
	Logfile    VARCHAR(1024) NOT NULL,
	Logline    VARCHAR(3072) NOT NULL,
	regexMatch BOOL          NOT NULL
);

-- Primary key
CREATE UNIQUE INDEX I010_T100_LinesSeen ON T100_LinesSeen(
	Logline
);

CREATE UNIQUE INDEX I020_T100_LinesSeen ON T100_LinesSeen(
	regexMatch,
	Logline
);

--	-- Index for cleanup
--	CREATE UNIQUE INDEX I020_T100_LinesSeen ON T100_LinesSeen(
--		Timestamp
--	);

---------------
-- V030_Drop --
---------------
-- Helper view to select set of new addresses to write iptables rules.
DROP VIEW IF EXISTS V030_Drop_010;
CREATE VIEW V030_Drop_010 AS
SELECT DISTINCT
	Octect_1,
	Octect_2,
	Octect_3,
	Octect_4
FROM T030_Drop
WHERE Status = 'D';
-- /*END*/
