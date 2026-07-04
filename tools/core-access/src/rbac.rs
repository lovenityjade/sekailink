#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Role {
    Admin,
    Service,
}

impl Role {
    pub fn parse(value: &str) -> Option<Self> {
        match value.trim().to_ascii_lowercase().as_str() {
            "admin" => Some(Self::Admin),
            "service" | "moderator" | "moderateur" | "mod" => Some(Self::Service),
            _ => None,
        }
    }

    pub fn as_str(self) -> &'static str {
        match self {
            Self::Admin => "admin",
            Self::Service => "service",
        }
    }

    pub fn allows(self, required: Role) -> bool {
        match (self, required) {
            (Self::Admin, _) => true,
            (Self::Service, Self::Service) => true,
            (Self::Service, Self::Admin) => false,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::Role;

    #[test]
    fn admin_allows_admin_and_service() {
        assert!(Role::Admin.allows(Role::Admin));
        assert!(Role::Admin.allows(Role::Service));
    }

    #[test]
    fn service_does_not_allow_admin() {
        assert!(!Role::Service.allows(Role::Admin));
        assert!(Role::Service.allows(Role::Service));
    }
}
