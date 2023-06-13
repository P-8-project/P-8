use crate::output::report_unhandled_error;
use crate::{report_error, report_info, Args};
use clap::CommandFactory;
use std::process;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum CliError {
    #[error("unable read file at path {0}")]
    ReadFileError(String),
    #[error("{0}")]
    ParseError(String),
    #[error("{0}")]
    WriteFileError(String),
    #[error("unable to read input")]
    ReadInputError,
    #[error("unable to read stdin")]
    ReadStdinError,
    #[error("repl argument supplied with piped input or output")]
    ReplWithPipe,
    #[error("No input file supplied and no input piped\nTry adding a path argument: 'viable ./file.mdy'")]
    StdinWithoutPipe,
    #[error("could not compile regex\nCause: {0}\nRegex: {1}")]
    CompileRegex(String, String),
}

#[derive(Debug)]
pub enum ErrorKind {
    Info,
    Error,
}

impl CliError {
    pub fn kind(&self) -> ErrorKind {
        match self {
            CliError::StdinWithoutPipe => ErrorKind::Info,
            _ => ErrorKind::Error,
        }
    }

    fn report(&self) {
        match self.kind() {
            ErrorKind::Info => {
                report_info(&self.to_string());
                println!();
                // silently ignoring an error when printing help
                // as we're already handling an error and have printed info
                let _print_result = Args::command().print_help();
            }
            ErrorKind::Error => {
                report_error(&self.to_string());
            }
        };
    }

    fn to_exit_code(&self) -> exitcode::ExitCode {
        match self {
            CliError::WriteFileError(_)
            | CliError::ReadFileError(_)
            | CliError::ReadInputError
            | CliError::ReadStdinError => exitcode::IOERR,
            CliError::ParseError(_) => exitcode::DATAERR,
            CliError::ReplWithPipe => exitcode::USAGE,
            CliError::StdinWithoutPipe => exitcode::NOINPUT,
            CliError::CompileRegex(_, _) => exitcode::DATAERR,
        }
    }
}

pub fn handle_error(error: &anyhow::Error) -> ! {
    let cli_error = error.downcast_ref::<CliError>();

    let cli_error = match cli_error {
        Some(cli_error) => cli_error,
        None => {
            report_unhandled_error(&error.to_string());
            process::exit(exitcode::SOFTWARE);
        }
    };

    cli_error.report();

    process::exit(cli_error.to_exit_code())
}
